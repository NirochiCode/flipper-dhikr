#include "include/profiles.h"

#include <string.h>

typedef struct {
    const char* id;
    const char* name;
    bool name_editable;
} DhikrProfileTemplate;

/*
 * Built-in profiles. Append new entries at the end; ids must never change
 * because they are the keys in the save file. All targets default to
 * DHIKR_TARGET_DEFAULT and are user-configurable.
 */
static const DhikrProfileTemplate dhikr_profile_templates[] = {
    {"subhanallah", "SubhanAllah", false},
    {"alhamdulillah", "Alhamdulillah", false},
    {"allahu_akbar", "Allahu Akbar", false},
    {"custom", "Custom Dhikr", true},
};

#define DHIKR_TEMPLATE_COUNT (sizeof(dhikr_profile_templates) / sizeof(dhikr_profile_templates[0]))

_Static_assert(DHIKR_TEMPLATE_COUNT <= DHIKR_PROFILE_MAX, "Too many built-in profiles");

void dhikr_state_init_defaults(DhikrState* state) {
    if(state == NULL) return;
    memset(state, 0, sizeof(*state));

    for(size_t i = 0; i < DHIKR_TEMPLATE_COUNT; i++) {
        DhikrProfile* profile = &state->profiles[i];
        profile->id = dhikr_profile_templates[i].id;
        strncpy(profile->name, dhikr_profile_templates[i].name, DHIKR_PROFILE_NAME_MAX);
        profile->name[DHIKR_PROFILE_NAME_MAX] = '\0';
        profile->name_editable = dhikr_profile_templates[i].name_editable;
        dhikr_counter_init(&profile->counter, DHIKR_TARGET_DEFAULT);
    }

    state->profile_count = (uint8_t)DHIKR_TEMPLATE_COUNT;
    state->active = 0;
    state->feedback = DHIKR_FEEDBACK_DEFAULT;
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
        if(state->profiles[i].id != NULL && strcmp(state->profiles[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

void dhikr_state_reset_active(DhikrState* state) {
    DhikrProfile* profile = dhikr_state_active(state);
    if(profile != NULL) dhikr_counter_reset(&profile->counter);
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

bool dhikr_profile_rename(DhikrProfile* profile, const char* name) {
    if(profile == NULL || !profile->name_editable || !dhikr_profile_name_is_valid(name)) {
        return false;
    }

    const char* start = name;
    while(*start == ' ')
        start++;
    size_t length = strlen(start);
    while(length > 0 && start[length - 1] == ' ')
        length--;

    memcpy(profile->name, start, length);
    profile->name[length] = '\0';
    return true;
}

bool dhikr_state_is_valid(const DhikrState* state) {
    if(state == NULL) return false;
    if(state->profile_count == 0 || state->profile_count > DHIKR_PROFILE_MAX) return false;
    if(state->active >= state->profile_count) return false;
    if(!dhikr_feedback_mode_is_valid((uint32_t)state->feedback)) return false;

    for(uint8_t i = 0; i < state->profile_count; i++) {
        const DhikrProfile* profile = &state->profiles[i];
        if(profile->id == NULL) return false;
        if(!dhikr_profile_name_is_valid(profile->name)) return false;
        if(!dhikr_counter_is_valid(&profile->counter)) return false;
    }
    return true;
}
