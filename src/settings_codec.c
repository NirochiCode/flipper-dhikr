#include "include/settings_codec.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CODEC_LINE_MAX 128
#define CODEC_CRC_KEY "crc32="
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

    for(uint8_t i = 0; i < state->profile_count; i++) {
        const DhikrProfile* profile = &state->profiles[i];
        writer_printf(
            &writer,
            CODEC_PROFILE_PREFIX "%s.count=%lu\n",
            profile->id,
            (unsigned long)profile->counter.count);
        writer_printf(
            &writer,
            CODEC_PROFILE_PREFIX "%s.target=%lu\n",
            profile->id,
            (unsigned long)profile->counter.target);
        if(profile->name_editable) {
            writer_printf(&writer, CODEC_PROFILE_PREFIX "%s.name=%s\n", profile->id, profile->name);
        }
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

typedef struct {
    const char* data;
    size_t length;
    size_t pos;
} LineReader;

/**
 * Return the next line (without "\n" / "\r\n").
 * `line_start` receives the offset of the line inside the data.
 */
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

static int find_profile(const DhikrState* state, Span id) {
    for(uint8_t i = 0; i < state->profile_count; i++) {
        if(span_equals(id, state->profiles[i].id)) return i;
    }
    return -1;
}

/** Apply a "profile.<id>.<field>=<value>" entry. */
static bool apply_profile_entry(DhikrState* state, Span key, Span value) {
    Span rest = {key.ptr + strlen(CODEC_PROFILE_PREFIX), key.len - strlen(CODEC_PROFILE_PREFIX)};

    size_t dot = rest.len;
    while(dot > 0 && rest.ptr[dot - 1] != '.')
        dot--;
    if(dot <= 1) return false; // no "<id>." part
    Span id = {rest.ptr, dot - 1};
    Span field = {rest.ptr + dot, rest.len - dot};

    int index = find_profile(state, id);
    if(index < 0) return true; // profile from another version: ignore
    DhikrProfile* profile = &state->profiles[index];

    if(span_equals(field, "count")) {
        uint32_t count;
        if(!span_parse_u32(value, &count) || count > DHIKR_COUNT_MAX) return false;
        profile->counter.count = count;
    } else if(span_equals(field, "target")) {
        uint32_t target;
        if(!span_parse_u32(value, &target) || !dhikr_target_is_valid(target)) return false;
        profile->counter.target = target;
    } else if(span_equals(field, "name")) {
        if(value.len == 0 || value.len > DHIKR_PROFILE_NAME_MAX) return false;
        char name[DHIKR_PROFILE_NAME_SIZE];
        memcpy(name, value.ptr, value.len);
        name[value.len] = '\0';
        if(!dhikr_profile_name_is_valid(name)) return false;
        if(profile->name_editable) {
            memcpy(profile->name, name, value.len + 1);
        }
    }
    // Unknown fields are ignored for forward compatibility.
    return true;
}

/* ------------------------------------------------------------------------- */
/* Parsing                                                                   */
/* ------------------------------------------------------------------------- */

static DhikrCodecResult parse_into(const char* data, size_t length, DhikrState* state) {
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
    if(!span_parse_u32(version_value, &version) || version == 0) return DhikrCodecCorrupt;
    if(version > DHIKR_SETTINGS_VERSION) return DhikrCodecUnsupportedVersion;
    size_t body_start = reader.pos;

    // 3. Locate the checksum line: the last non-empty line.
    size_t crc_line_start = 0;
    Span crc_line = {NULL, 0};
    {
        LineReader scan = reader;
        size_t start;
        while(reader_next(&scan, &line, &start)) {
            if(line.len > 0) {
                crc_line = line;
                crc_line_start = start;
            }
        }
    }
    if(crc_line.ptr == NULL || !span_starts_with(crc_line, CODEC_CRC_KEY)) {
        return DhikrCodecCorrupt;
    }
    uint32_t stored_crc;
    Span crc_value = {crc_line.ptr + strlen(CODEC_CRC_KEY), crc_line.len - strlen(CODEC_CRC_KEY)};
    if(!span_parse_hex32(crc_value, &stored_crc)) return DhikrCodecCorrupt;
    if(dhikr_crc32(data, crc_line_start) != stored_crc) return DhikrCodecCorrupt;

    // 4. Key/value body
    bool has_active = false;
    bool has_feedback = false;
    reader.pos = body_start;
    size_t start;
    while(reader_next(&reader, &line, &start) && start < crc_line_start) {
        if(line.len == 0) continue;
        if(line.len > CODEC_LINE_MAX) return DhikrCodecCorrupt;

        const char* eq = memchr(line.ptr, '=', line.len);
        if(eq == NULL || eq == line.ptr) return DhikrCodecCorrupt;
        Span key = {line.ptr, (size_t)(eq - line.ptr)};
        Span value = {eq + 1, line.len - key.len - 1};

        if(span_equals(key, "active")) {
            int index = find_profile(state, value);
            // An unknown id (e.g. a profile removed in a later version) keeps index 0.
            state->active = index >= 0 ? (uint8_t)index : 0;
            has_active = true;
        } else if(span_equals(key, "feedback")) {
            uint32_t mode;
            if(!span_parse_u32(value, &mode) || !dhikr_feedback_mode_is_valid(mode)) {
                return DhikrCodecCorrupt;
            }
            state->feedback = (DhikrFeedbackMode)mode;
            has_feedback = true;
        } else if(span_starts_with(key, CODEC_PROFILE_PREFIX)) {
            if(!apply_profile_entry(state, key, value)) return DhikrCodecCorrupt;
        }
        // Other keys are ignored for forward compatibility.
    }

    if(!has_active || !has_feedback) return DhikrCodecCorrupt;
    return dhikr_state_is_valid(state) ? DhikrCodecOk : DhikrCodecCorrupt;
}

DhikrCodecResult dhikr_settings_parse(const char* data, size_t length, DhikrState* out) {
    DhikrState parsed;
    dhikr_state_init_defaults(&parsed);

    DhikrCodecResult result = parse_into(data, length, &parsed);

    if(out != NULL) {
        if(result == DhikrCodecOk) {
            *out = parsed;
        } else {
            dhikr_state_init_defaults(out);
        }
    }
    return result;
}
