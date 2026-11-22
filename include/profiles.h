/**
 * @file profiles.h
 * @brief Dhikr profiles and the complete persisted application state.
 *
 * Every profile has a stable `id` that is used as the storage key, so new
 * built-in profiles can be added (or the display order changed) in future
 * versions without breaking existing save files.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "include/counter.h"
#include "include/feedback.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of profiles the state can hold (room for future profiles). */
#define DHIKR_PROFILE_MAX 8
/** Maximum profile name length in characters, excluding the terminator. */
#define DHIKR_PROFILE_NAME_MAX 16
/** Buffer size required for a profile name. */
#define DHIKR_PROFILE_NAME_SIZE (DHIKR_PROFILE_NAME_MAX + 1)

typedef struct {
    /** Stable storage key, points to a string literal. */
    const char* id;
    char name[DHIKR_PROFILE_NAME_SIZE];
    DhikrCounter counter;
    bool name_editable;
} DhikrProfile;

typedef struct {
    DhikrProfile profiles[DHIKR_PROFILE_MAX];
    uint8_t profile_count;
    uint8_t active;
    DhikrFeedbackMode feedback;
} DhikrState;

/** Fill `state` with the built-in profiles and default settings. */
void dhikr_state_init_defaults(DhikrState* state);

/** @return the active profile (never NULL for a valid state). */
DhikrProfile* dhikr_state_active(DhikrState* state);
const DhikrProfile* dhikr_state_active_const(const DhikrState* state);

/** Make profile `index` active. Counters are not modified. */
bool dhikr_state_select(DhikrState* state, uint8_t index);

/** Activate the next (`step` > 0) or previous (`step` < 0) profile, wrapping around. */
void dhikr_state_cycle(DhikrState* state, int step);

/** @return index of the profile with the given id, or -1. */
int dhikr_state_find(const DhikrState* state, const char* id);

/** Reset only the active profile's counter to 0. */
void dhikr_state_reset_active(DhikrState* state);

/**
 * A valid name is 1..DHIKR_PROFILE_NAME_MAX printable ASCII characters and
 * not only spaces. Surrounding spaces are allowed here; rename trims them.
 */
bool dhikr_profile_name_is_valid(const char* name);

/**
 * Rename an editable profile. Leading/trailing spaces are trimmed.
 * @return false (profile unchanged) if not editable or the name is invalid.
 */
bool dhikr_profile_rename(DhikrProfile* profile, const char* name);

/** Full consistency check of a state, e.g. after loading. */
bool dhikr_state_is_valid(const DhikrState* state);

#ifdef __cplusplus
}
#endif
