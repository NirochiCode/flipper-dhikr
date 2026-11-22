#include "include/profiles.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char* id;
    const char* name;
} DhikrBuiltinTemplate;

/*
 * Built-in profiles. Their ids are storage keys and must never change.
 * All targets default to DHIKR_TARGET_DEFAULT and are user-configurable.
 */
static const DhikrBuiltinTemplate dhikr_builtin_templates[DHIKR_BUILTIN_PROFILE_COUNT] = {
    {"subhanallah", "SubhanAllah"},
    {"alhamdulillah", "Alhamdulillah"},
    {"allahu_akbar", "Allahu Akbar"},
};

_Static_assert(DHIKR_BUILTIN_PROFILE_COUNT < DHIKR_PROFILE_MAX, "No room for custom profiles");

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static void copy_str(char* dst, size_t dst_size, const char* src) {
    snprintf(dst, dst_size, "%s", src);
}

static void profile_init(DhikrProfile* profile, const char* id, const char* name, bool builtin) {
    memset(profile, 0, sizeof(*profile));
    copy_str(profile->id, sizeof(profile->id), id);
    copy_str(profile->name, sizeof(profile->name), name);
    profile->builtin = builtin;
    dhikr_counter_init(&profile->counter, DHIKR_TARGET_DEFAULT);
}

/** Trimmed view of a string (no copy). */
static void trim_span(const char* text, const char** start, size_t* length) {
    const char* s = text;
    while(*s == ' ')
        s++;
    size_t len = strlen(s);
    while(len > 0 && s[len - 1] == ' ')
        len--;
    *start = s;
    *length = len;
}

static char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/** Case-insensitive comparison of two names, ignoring surrounding spaces. */
static bool names_equal(const char* a, const char* b) {
    const char* sa;
    const char* sb;
    size_t la, lb;
    trim_span(a, &sa, &la);
    trim_span(b, &sb, &lb);
    if(la != lb) return false;
    for(size_t i = 0; i < la; i++) {
        if(ascii_lower(sa[i]) != ascii_lower(sb[i])) return false;
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/* State                                                                     */
/* ------------------------------------------------------------------------- */

void dhikr_state_init_defaults(DhikrState* state) {
    if(state == NULL) return;
    memset(state, 0, sizeof(*state));

    for(size_t i = 0; i < DHIKR_BUILTIN_PROFILE_COUNT; i++) {
        profile_init(
            &state->profiles[i], dhikr_builtin_templates[i].id, dhikr_builtin_templates[i].name, true);
    }
    profile_init(
        &state->profiles[DHIKR_BUILTIN_PROFILE_COUNT],
        DHIKR_LEGACY_CUSTOM_ID,
        DHIKR_LEGACY_CUSTOM_NAME,
        false);

    state->profile_count = DHIKR_BUILTIN_PROFILE_COUNT + 1;
    state->active = 0;
    dhikr_feedback_settings_init_defaults(&state->feedback);
    state->next_custom_id = 1;
}

DhikrProfile* dhikr_state_active(DhikrState* state) {
    if(state == NULL || state->profile_count == 0) return NULL;
    if(state->active >= state->profile_count) state->active = 0;
    return &state->profiles[state->active];
}

const DhikrProfile* dhikr_state_active_const(const DhikrState* state) {
    if(state == NULL || state->profile_count == 0) return NULL;
    uint8_t index = state->active < state->profile_count ? state->active : 0;
    return &state->profiles[index];
}

bool dhikr_state_select(DhikrState* state, uint8_t index) {
    if(state == NULL || index >= state->profile_count) return false;
    state->active = index;
    return true;
}

void dhikr_state_cycle(DhikrState* state, int step) {
    if(state == NULL || state->profile_count == 0 || step == 0) return;
    int count = state->profile_count;
    int next = ((int)state->active + (step % count) + count) % count;
    state->active = (uint8_t)next;
}

int dhikr_state_find(const DhikrState* state, const char* id) {
    if(state == NULL || id == NULL) return -1;
    for(uint8_t i = 0; i < state->profile_count; i++) {
        if(strcmp(state->profiles[i].id, id) == 0) return i;
    }
    return -1;
}

void dhikr_state_reset_active(DhikrState* state) {
    DhikrProfile* profile = dhikr_state_active(state);
    if(profile != NULL) dhikr_counter_reset(&profile->counter);
}

bool dhikr_state_can_add(const DhikrState* state) {
    return state != NULL && state->profile_count < DHIKR_PROFILE_MAX &&
           state->next_custom_id <= DHIKR_CUSTOM_ID_MAX;
}

DhikrProfileError
    dhikr_state_check_name(const DhikrState* state, const char* name, int exclude_index) {
    if(state == NULL || !dhikr_profile_name_is_valid(name)) return DhikrProfileErrorInvalidName;
    for(uint8_t i = 0; i < state->profile_count; i++) {
        if((int)i == exclude_index) continue;
        if(names_equal(state->profiles[i].name, name)) return DhikrProfileErrorDuplicateName;
    }
    return DhikrProfileOk;
}

/** Copy `name` trimmed into the profile. The name must already be valid. */
static void profile_set_name(DhikrProfile* profile, const char* name) {
    const char* start;
    size_t length;
    trim_span(name, &start, &length);
    memcpy(profile->name, start, length);
    profile->name[length] = '\0';
}

DhikrProfileError dhikr_state_add_custom(
    DhikrState* state,
    const char* name,
    uint32_t target,
    uint8_t* new_index) {
    if(state == NULL) return DhikrProfileErrorInvalid;
    if(!dhikr_state_can_add(state)) return DhikrProfileErrorFull;
    if(!dhikr_target_is_valid(target)) return DhikrProfileErrorInvalid;
    DhikrProfileError error = dhikr_state_check_name(state, name, -1);
    if(error != DhikrProfileOk) return error;

    char id[DHIKR_PROFILE_ID_SIZE];
    snprintf(id, sizeof(id), "c%lu", (unsigned long)state->next_custom_id);
    if(dhikr_state_find(state, id) >= 0) return DhikrProfileErrorInvalid; // cannot happen

    uint8_t index = state->profile_count;
    DhikrProfile* profile = &state->profiles[index];
    profile_init(profile, id, "", false);
    profile_set_name(profile, name);
    dhikr_counter_set_target(&profile->counter, target);

    state->profile_count++;
    state->next_custom_id++;
    if(new_index != NULL) *new_index = index;
    return DhikrProfileOk;
}

DhikrProfileError dhikr_state_rename(DhikrState* state, uint8_t index, const char* name) {
    if(state == NULL || index >= state->profile_count) return DhikrProfileErrorInvalid;
    DhikrProfile* profile = &state->profiles[index];
    if(profile->builtin) return DhikrProfileErrorBuiltin;
    DhikrProfileError error = dhikr_state_check_name(state, name, index);
    if(error != DhikrProfileOk) return error;
    profile_set_name(profile, name);
    return DhikrProfileOk;
}

DhikrProfileError dhikr_state_delete(DhikrState* state, uint8_t index) {
    if(state == NULL || index >= state->profile_count) return DhikrProfileErrorInvalid;
    if(state->profiles[index].builtin) return DhikrProfileErrorBuiltin;

    for(uint8_t i = index; i + 1 < state->profile_count; i++) {
        state->profiles[i] = state->profiles[i + 1];
    }
    state->profile_count--;
    memset(&state->profiles[state->profile_count], 0, sizeof(DhikrProfile));

    if(state->active == index) {
        // The deleted profile was active: fall back to the one before it.
        state->active = index > 0 ? (uint8_t)(index - 1) : 0;
    } else if(state->active > index) {
        state->active--; // keep the same profile active
    }
    return DhikrProfileOk;
}

void dhikr_state_suggest_name(const DhikrState* state, char* out, size_t out_size) {
    if(out == NULL || out_size == 0) return;
    for(unsigned n = 1; n <= DHIKR_PROFILE_MAX + 1; n++) {
        snprintf(out, out_size, "Dhikr %u", n);
        if(dhikr_state_check_name(state, out, -1) == DhikrProfileOk) return;
    }
}

void dhikr_state_profile_label(const DhikrState* state, uint8_t index, char* out, size_t out_size) {
    if(out == NULL || out_size == 0) return;
    out[0] = '\0';
    if(state == NULL || index >= state->profile_count) return;
    snprintf(
        out, out_size, "%s%s", index == state->active ? "* " : "  ", state->profiles[index].name);
}

bool dhikr_profile_name_is_valid(const char* name) {
    if(name == NULL) return false;
    size_t length = 0;
    bool has_visible = false;
    for(const char* p = name; *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;
        if(c < 0x20 || c > 0x7E) return false;
        if(c != ' ') has_visible = true;
        if(++length > DHIKR_PROFILE_NAME_MAX) return false;
    }
    return has_visible;
}

uint32_t dhikr_profile_custom_id_number(const char* id) {
    if(id == NULL || id[0] != 'c' || id[1] < '1' || id[1] > '9') return 0;
    uint64_t value = 0;
    size_t digits = 0;
    for(const char* p = id + 1; *p != '\0'; p++) {
        if(*p < '0' || *p > '9' || ++digits > 9) return 0;
        value = value * 10U + (uint64_t)(*p - '0');
    }
    return value <= DHIKR_CUSTOM_ID_MAX ? (uint32_t)value : 0;
}

bool dhikr_profile_id_is_custom(const char* id) {
    if(id == NULL) return false;
    return strcmp(id, DHIKR_LEGACY_CUSTOM_ID) == 0 || dhikr_profile_custom_id_number(id) > 0;
}

bool dhikr_state_is_valid(const DhikrState* state) {
    if(state == NULL) return false;
    if(state->profile_count < DHIKR_BUILTIN_PROFILE_COUNT ||
       state->profile_count > DHIKR_PROFILE_MAX) {
        return false;
    }
    if(state->active >= state->profile_count) return false;
    if(state->next_custom_id == 0) return false;

    for(uint8_t i = 0; i < state->profile_count; i++) {
        const DhikrProfile* profile = &state->profiles[i];
        if(i < DHIKR_BUILTIN_PROFILE_COUNT) {
            if(!profile->builtin || strcmp(profile->id, dhikr_builtin_templates[i].id) != 0) {
                return false;
            }
        } else {
            if(profile->builtin || !dhikr_profile_id_is_custom(profile->id)) return false;
            uint32_t number = dhikr_profile_custom_id_number(profile->id);
            if(number >= state->next_custom_id) return false; // id could be handed out again
        }
        if(!dhikr_profile_name_is_valid(profile->name)) return false;
        if(!dhikr_counter_is_valid(&profile->counter)) return false;
        for(uint8_t j = 0; j < i; j++) {
            if(strcmp(state->profiles[j].id, profile->id) == 0) return false;
        }
    }
    return true;
}
