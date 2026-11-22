#include "include/settings_codec.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CODEC_LINE_MAX       256
#define CODEC_CRC_KEY        "crc32="
#define CODEC_PROFILE_PREFIX "profile."

/* ------------------------------------------------------------------------- */
/* CRC-32                                                                    */
/* ------------------------------------------------------------------------- */

uint32_t dhikr_crc32(const void* data, size_t length) {
    const uint8_t* bytes = data;
    uint32_t crc = 0xFFFFFFFFUL;
    for(size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for(int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
        }
    }
    return ~crc;
}

/* ------------------------------------------------------------------------- */
/* Serialization                                                             */
/* ------------------------------------------------------------------------- */

typedef struct {
    char* buffer;
    size_t capacity;
    size_t length;
    bool overflow;
} CodecWriter;

static void writer_printf(CodecWriter* writer, const char* format, ...) {
    if(writer->overflow) return;
    va_list args;
    va_start(args, format);
    int written = vsnprintf(
        writer->buffer + writer->length, writer->capacity - writer->length, format, args);
    va_end(args);
    if(written < 0 || (size_t)written >= writer->capacity - writer->length) {
        writer->overflow = true;
        return;
    }
    writer->length += (size_t)written;
}

size_t dhikr_settings_serialize(const DhikrState* state, char* buffer, size_t capacity) {
    if(buffer == NULL || capacity == 0) return 0;
    buffer[0] = '\0';
    if(!dhikr_state_is_valid(state)) return 0;

    CodecWriter writer = {.buffer = buffer, .capacity = capacity};
    const DhikrProfile* active = dhikr_state_active_const(state);

    writer_printf(&writer, "%s\n", DHIKR_SETTINGS_MAGIC);
    writer_printf(&writer, "version=%lu\n", (unsigned long)DHIKR_SETTINGS_VERSION);
    writer_printf(&writer, "active=%s\n", active->id);
    writer_printf(&writer, "feedback=%lu\n", (unsigned long)state->feedback);
    writer_printf(&writer, "next_custom_id=%lu\n", (unsigned long)state->next_custom_id);

    writer_printf(&writer, "custom_profiles=");
    bool first = true;
    for(uint8_t i = DHIKR_BUILTIN_PROFILE_COUNT; i < state->profile_count; i++) {
        writer_printf(&writer, "%s%s", first ? "" : ",", state->profiles[i].id);
        first = false;
    }
    writer_printf(&writer, "\n");

    for(uint8_t i = 0; i < state->profile_count; i++) {
        const DhikrProfile* p = &state->profiles[i];
        if(!p->builtin) {
            writer_printf(&writer, CODEC_PROFILE_PREFIX "%s.name=%s\n", p->id, p->name);
        }
        writer_printf(
            &writer, CODEC_PROFILE_PREFIX "%s.count=%lu\n", p->id, (unsigned long)p->counter.count);
        writer_printf(
            &writer,
            CODEC_PROFILE_PREFIX "%s.target=%lu\n",
            p->id,
            (unsigned long)p->counter.target);
        writer_printf(
            &writer,
            CODEC_PROFILE_PREFIX "%s.rounds=%lu\n",
            p->id,
            (unsigned long)p->counter.rounds);
    }

    if(writer.overflow) {
        buffer[0] = '\0';
        return 0;
    }

    uint32_t crc = dhikr_crc32(buffer, writer.length);
    writer_printf(&writer, CODEC_CRC_KEY "%08lX\n", (unsigned long)crc);

    if(writer.overflow) {
        buffer[0] = '\0';
        return 0;
    }
    return writer.length;
}

/* ------------------------------------------------------------------------- */
/* Parsing helpers                                                           */
/* ------------------------------------------------------------------------- */

typedef struct {
    const char* ptr;
    size_t len;
} Span;

static bool span_equals(Span span, const char* literal) {
    size_t literal_len = strlen(literal);
    return span.len == literal_len && memcmp(span.ptr, literal, literal_len) == 0;
}

static bool span_starts_with(Span span, const char* prefix) {
    size_t prefix_len = strlen(prefix);
    return span.len >= prefix_len && memcmp(span.ptr, prefix, prefix_len) == 0;
}

/** Strict unsigned decimal: 1..10 digits, no sign, no spaces, no overflow. */
static bool span_parse_u32(Span span, uint32_t* out) {
    if(span.len == 0 || span.len > 10) return false;
    uint64_t value = 0;
    for(size_t i = 0; i < span.len; i++) {
        char c = span.ptr[i];
        if(c < '0' || c > '9') return false;
        value = value * 10U + (uint64_t)(c - '0');
    }
    if(value > UINT32_MAX) return false;
    *out = (uint32_t)value;
    return true;
}

/** Exactly 8 hex digits (either case). */
static bool span_parse_hex32(Span span, uint32_t* out) {
    if(span.len != 8) return false;
    uint32_t value = 0;
    for(size_t i = 0; i < span.len; i++) {
        char c = span.ptr[i];
        uint32_t nibble;
        if(c >= '0' && c <= '9') {
            nibble = (uint32_t)(c - '0');
        } else if(c >= 'A' && c <= 'F') {
            nibble = (uint32_t)(c - 'A' + 10);
        } else if(c >= 'a' && c <= 'f') {
            nibble = (uint32_t)(c - 'a' + 10);
        } else {
            return false;
        }
        value = (value << 4) | nibble;
    }
    *out = value;
    return true;
}

/** Copy a span into a NUL-terminated buffer; false if it does not fit. */
static bool span_copy(Span span, char* out, size_t out_size) {
    if(span.len + 1 > out_size) return false;
    memcpy(out, span.ptr, span.len);
    out[span.len] = '\0';
    return true;
}

typedef struct {
    const char* data;
    size_t length;
    size_t pos;
} LineReader;

/** Return the next line (without "\n" / "\r\n"); `line_start` gets its offset. */
static bool reader_next(LineReader* reader, Span* line, size_t* line_start) {
    if(reader->pos >= reader->length) return false;
    size_t start = reader->pos;
    size_t end = start;
    while(end < reader->length && reader->data[end] != '\n')
        end++;
    reader->pos = end < reader->length ? end + 1 : end;

    size_t len = end - start;
    if(len > 0 && reader->data[start + len - 1] == '\r') len--;
    line->ptr = reader->data + start;
    line->len = len;
    if(line_start) *line_start = start;
    return true;
}

/** Split "key=value"; false if there is no '=' or the key is empty. */
static bool split_key_value(Span line, Span* key, Span* value) {
    const char* eq = memchr(line.ptr, '=', line.len);
    if(eq == NULL || eq == line.ptr) return false;
    key->ptr = line.ptr;
    key->len = (size_t)(eq - line.ptr);
    value->ptr = eq + 1;
    value->len = line.len - key->len - 1;
    return true;
}

static int find_profile(const DhikrState* state, Span id) {
    for(uint8_t i = 0; i < state->profile_count; i++) {
        if(span_equals(id, state->profiles[i].id)) return i;
    }
    return -1;
}

/* ------------------------------------------------------------------------- */
/* Parsing                                                                   */
/* ------------------------------------------------------------------------- */

typedef struct {
    bool has_name;
    bool has_count;
    bool has_target;
} ProfileSeen;

typedef struct {
    const char* data;
    size_t body_start;
    size_t body_end; // start of the checksum line
} Body;

/** Iterate non-empty body lines; returns false at the end. Corrupt lines set *error. */
static bool body_next(LineReader* reader, const Body* body, Span* key, Span* value, bool* error) {
    Span line;
    size_t start;
    while(reader_next(reader, &line, &start) && start < body->body_end) {
        if(line.len == 0) continue;
        if(line.len > CODEC_LINE_MAX || !split_key_value(line, key, value)) {
            *error = true;
            return false;
        }
        return true;
    }
    return false;
}

/** Start from the built-ins only and append a custom profile slot per listed id. */
static bool apply_custom_list(DhikrState* state, Span list) {
    state->profile_count = DHIKR_BUILTIN_PROFILE_COUNT;
    if(list.len == 0) return true;

    size_t pos = 0;
    while(pos <= list.len) {
        size_t end = pos;
        while(end < list.len && list.ptr[end] != ',')
            end++;
        Span id_span = {list.ptr + pos, end - pos};
        char id[DHIKR_PROFILE_ID_SIZE];
        if(!span_copy(id_span, id, sizeof(id))) return false;
        if(!dhikr_profile_id_is_custom(id)) return false;
        if(find_profile(state, id_span) >= 0) return false; // duplicate id
        if(state->profile_count >= DHIKR_PROFILE_MAX) return false;

        DhikrProfile* profile = &state->profiles[state->profile_count++];
        memset(profile, 0, sizeof(*profile));
        memcpy(profile->id, id, strlen(id) + 1);
        profile->builtin = false;
        dhikr_counter_init(&profile->counter, DHIKR_TARGET_DEFAULT);

        pos = end + 1;
    }
    return true;
}

/** Apply "profile.<id>.<field>=<value>". */
static bool apply_profile_entry(DhikrState* state, ProfileSeen* seen, Span key, Span value) {
    Span rest = {key.ptr + strlen(CODEC_PROFILE_PREFIX), key.len - strlen(CODEC_PROFILE_PREFIX)};

    size_t dot = rest.len;
    while(dot > 0 && rest.ptr[dot - 1] != '.')
        dot--;
    if(dot <= 1) return false; // no "<id>." part
    Span id = {rest.ptr, dot - 1};
    Span field = {rest.ptr + dot, rest.len - dot};

    int index = find_profile(state, id);
    if(index < 0) return true; // profile not listed / from another version: ignore
    DhikrProfile* profile = &state->profiles[index];

    if(span_equals(field, "count")) {
        uint32_t count;
        if(!span_parse_u32(value, &count) || count > DHIKR_COUNT_MAX) return false;
        profile->counter.count = count;
        seen[index].has_count = true;
    } else if(span_equals(field, "target")) {
        uint32_t target;
        if(!span_parse_u32(value, &target) || !dhikr_target_is_valid(target)) return false;
        profile->counter.target = target;
        seen[index].has_target = true;
    } else if(span_equals(field, "rounds")) {
        uint32_t rounds;
        if(!span_parse_u32(value, &rounds) || rounds > DHIKR_ROUNDS_MAX) return false;
        profile->counter.rounds = rounds;
    } else if(span_equals(field, "name")) {
        char name[DHIKR_PROFILE_NAME_SIZE];
        if(value.len == 0 || !span_copy(value, name, sizeof(name))) return false;
        if(!dhikr_profile_name_is_valid(name)) return false;
        if(!profile->builtin) {
            memcpy(profile->name, name, strlen(name) + 1);
            seen[index].has_name = true;
        }
    }
    // Unknown fields are ignored for forward compatibility.
    return true;
}

static DhikrCodecResult
    parse_into(const char* data, size_t length, DhikrState* state, uint32_t* version_out) {
    if(data == NULL || length == 0 || length >= DHIKR_SETTINGS_BUFFER_SIZE) {
        return DhikrCodecCorrupt;
    }
    if(memchr(data, '\0', length) != NULL) return DhikrCodecCorrupt;

    LineReader reader = {.data = data, .length = length, .pos = 0};
    Span line;

    // 1. Magic
    if(!reader_next(&reader, &line, NULL) || !span_equals(line, DHIKR_SETTINGS_MAGIC)) {
        return DhikrCodecCorrupt;
    }

    // 2. Version (checked before the checksum: a future format may differ)
    uint32_t version;
    if(!reader_next(&reader, &line, NULL) || !span_starts_with(line, "version=")) {
        return DhikrCodecCorrupt;
    }
    Span version_value = {line.ptr + 8, line.len - 8};
    if(!span_parse_u32(version_value, &version) || version < DHIKR_SETTINGS_VERSION_MIN) {
        return DhikrCodecCorrupt;
    }
    *version_out = version;
    if(version > DHIKR_SETTINGS_VERSION) return DhikrCodecUnsupportedVersion;

    // 3. Checksum: the last non-empty line covers everything before it.
    Body body = {.data = data, .body_start = reader.pos, .body_end = 0};
    Span crc_line = {NULL, 0};
    {
        LineReader scan = reader;
        size_t start;
        while(reader_next(&scan, &line, &start)) {
            if(line.len > 0) {
                crc_line = line;
                body.body_end = start;
            }
        }
    }
    if(crc_line.ptr == NULL || !span_starts_with(crc_line, CODEC_CRC_KEY)) {
        return DhikrCodecCorrupt;
    }
    uint32_t stored_crc;
    Span crc_value = {crc_line.ptr + strlen(CODEC_CRC_KEY), crc_line.len - strlen(CODEC_CRC_KEY)};
    if(!span_parse_hex32(crc_value, &stored_crc)) return DhikrCodecCorrupt;
    if(dhikr_crc32(data, body.body_end) != stored_crc) return DhikrCodecCorrupt;

    // 4. Pass 1: settings and the list of custom profiles.
    Span key, value;
    Span active_id = {NULL, 0};
    bool has_feedback = false;
    bool has_custom_list = false;
    bool error = false;

    reader.pos = body.body_start;
    while(body_next(&reader, &body, &key, &value, &error)) {
        if(span_equals(key, "active")) {
            active_id = value;
        } else if(span_equals(key, "feedback")) {
            uint32_t mode;
            if(!span_parse_u32(value, &mode) || !dhikr_feedback_mode_is_valid(mode)) {
                return DhikrCodecCorrupt;
            }
            state->feedback = (DhikrFeedbackMode)mode;
            has_feedback = true;
        } else if(version >= 2 && span_equals(key, "next_custom_id")) {
            uint32_t next;
            if(!span_parse_u32(value, &next) || next == 0 || next > DHIKR_CUSTOM_ID_MAX + 1) {
                return DhikrCodecCorrupt;
            }
            state->next_custom_id = next;
        } else if(version >= 2 && span_equals(key, "custom_profiles")) {
            if(has_custom_list || !apply_custom_list(state, value)) return DhikrCodecCorrupt;
            has_custom_list = true;
        }
    }
    if(error || active_id.ptr == NULL || !has_feedback) return DhikrCodecCorrupt;
    if(version >= 2 && !has_custom_list) return DhikrCodecCorrupt;
    // Version 1: the default state already holds the single "custom" profile.

    // 5. Pass 2: profile values.
    ProfileSeen seen[DHIKR_PROFILE_MAX];
    memset(seen, 0, sizeof(seen));
    reader.pos = body.body_start;
    while(body_next(&reader, &body, &key, &value, &error)) {
        if(span_starts_with(key, CODEC_PROFILE_PREFIX)) {
            if(!apply_profile_entry(state, seen, key, value)) return DhikrCodecCorrupt;
        }
    }
    if(error) return DhikrCodecCorrupt;

    // 6. Completeness, migration and fix-ups.
    uint32_t max_custom_number = 0;
    for(uint8_t i = DHIKR_BUILTIN_PROFILE_COUNT; i < state->profile_count; i++) {
        DhikrProfile* profile = &state->profiles[i];
        if(version >= 2 &&
           (!seen[i].has_name || !seen[i].has_count || !seen[i].has_target)) {
            return DhikrCodecCorrupt;
        }
        uint32_t number = dhikr_profile_custom_id_number(profile->id);
        if(number > max_custom_number) max_custom_number = number;
    }
    if(max_custom_number >= state->next_custom_id) state->next_custom_id = max_custom_number + 1;

    if(version == 1) {
        // v1.0 kept counting past the target: fold the surplus into rounds.
        for(uint8_t i = 0; i < state->profile_count; i++) {
            dhikr_counter_normalize(&state->profiles[i].counter);
        }
    }

    // An unknown active id (e.g. a profile from a newer version) falls back to the first.
    int active = find_profile(state, active_id);
    state->active = active >= 0 ? (uint8_t)active : 0;

    return dhikr_state_is_valid(state) ? DhikrCodecOk : DhikrCodecCorrupt;
}

DhikrCodecResult dhikr_settings_parse(
    const char* data,
    size_t length,
    DhikrState* out,
    uint32_t* source_version) {
    DhikrState parsed;
    dhikr_state_init_defaults(&parsed);

    uint32_t version = 0;
    DhikrCodecResult result = parse_into(data, length, &parsed, &version);

    if(source_version != NULL) *source_version = version;
    if(out != NULL) {
        if(result == DhikrCodecOk) {
            *out = parsed;
        } else {
            dhikr_state_init_defaults(out);
        }
    }
    return result;
}
