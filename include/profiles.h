/**
 * @file profiles.h
 * @brief Dhikr profiles and the complete persisted application state.
 *
 * Built-in profiles (SubhanAllah, Alhamdulillah, Allahu Akbar) always come
 * first, keep their names and cannot be deleted. All other profiles are
 * custom: they can be added, renamed and deleted.
 *
 * Every profile has a stable `id` that is used as the storage key:
 *  - built-ins: "subhanallah", "alhamdulillah", "allahu_akbar"
 *  - the custom profile from v1.0: "custom"
 *  - custom profiles created since v1.0.1: "c1", "c2", ... taken from
 *    `next_custom_id`, which only ever increases, so ids are never reused.
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

/** Maximum number of profiles in total (built-in + custom). */
#define DHIKR_PROFILE_MAX 16
/** Maximum profile name length in characters, excluding the terminator. */
#define DHIKR_PROFILE_NAME_MAX 16
/** Buffer size required for a profile name. */
#define DHIKR_PROFILE_NAME_SIZE (DHIKR_PROFILE_NAME_MAX + 1)
/** Buffer size for a profile id ("c" + up to 9 digits, or a built-in id). */
#define DHIKR_PROFILE_ID_SIZE 16
/** Number of built-in, non-deletable profiles. */
#define DHIKR_BUILTIN_PROFILE_COUNT 3
/** Id of the single custom profile that existed in v1.0. */
#define DHIKR_LEGACY_CUSTOM_ID "custom"
/** Name of the default custom profile. */
#define DHIKR_LEGACY_CUSTOM_NAME "Custom Dhikr"
/** Highest number used in "c<N>" ids. */
#define DHIKR_CUSTOM_ID_MAX 999999999UL

typedef struct {
    char id[DHIKR_PROFILE_ID_SIZE];
    char name[DHIKR_PROFILE_NAME_SIZE];
    DhikrCounter counter;
    /** Built-in profiles cannot be renamed or deleted. */
    bool builtin;
} DhikrProfile;

typedef struct {
    DhikrProfile profiles[DHIKR_PROFILE_MAX];
    uint8_t profile_count;
    uint8_t active;
    /** Four independent feedback switches (v1.0.2). */
    DhikrFeedbackSettings feedback;
    /** Number for the next "c<N>" id; never decreases. */
    uint32_t next_custom_id;
} DhikrState;

typedef enum {
    DhikrProfileOk,
    /** Empty, only spaces, too long or non-printable characters. */
    DhikrProfileErrorInvalidName,
    /** Another profile already has this name (case-insensitive). */
    DhikrProfileErrorDuplicateName,
    /** DHIKR_PROFILE_MAX reached. */
    DhikrProfileErrorFull,
    /** Built-in profiles cannot be renamed or deleted. */
    DhikrProfileErrorBuiltin,
    /** Index out of range or no free id. */
    DhikrProfileErrorInvalid,
} DhikrProfileError;

/** Fill `state` with the built-in profiles, "Custom Dhikr" and default settings. */
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

/**
 * Change the target of profile `index` (not necessarily the active one).
 * The active profile, the count and the completed rounds (sets) stay unchanged.
 * @return true if the target was changed (false if invalid or already equal)
 */
bool dhikr_state_set_target(DhikrState* state, uint8_t index, uint32_t target);

/** Reset count and rounds of the active profile only. */
void dhikr_state_reset_active(DhikrState* state);

/** @return true if another profile can be added. */
bool dhikr_state_can_add(const DhikrState* state);

/**
 * Check a proposed name: valid characters/length and not used by another
 * profile (case-insensitive, surrounding spaces ignored). `exclude_index` is
 * the profile being renamed, or -1 when adding.
 */
DhikrProfileError
    dhikr_state_check_name(const DhikrState* state, const char* name, int exclude_index);

/**
 * Append a custom profile with a new unique id. The active profile is not changed.
 * @param[out] new_index index of the new profile on success (may be NULL)
 */
DhikrProfileError dhikr_state_add_custom(
    DhikrState* state,
    const char* name,
    uint32_t target,
    uint8_t* new_index);

/** Rename a custom profile (name is trimmed). */
DhikrProfileError dhikr_state_rename(DhikrState* state, uint8_t index, const char* name);

/**
 * Delete a custom profile. If it was active, the previous profile in the
 * list becomes active; otherwise the active profile stays the same.
 */
DhikrProfileError dhikr_state_delete(DhikrState* state, uint8_t index);

/** Write a free default name ("Dhikr 1", "Dhikr 2", ...) into `out`. */
void dhikr_state_suggest_name(const DhikrState* state, char* out, size_t out_size);

/**
 * A valid name is 1..DHIKR_PROFILE_NAME_MAX printable ASCII characters and
 * not only spaces.
 */
bool dhikr_profile_name_is_valid(const char* name);

/** @return true for "custom" or "c<1..DHIKR_CUSTOM_ID_MAX>" (no leading zeros). */
bool dhikr_profile_id_is_custom(const char* id);

/** @return N for "c<N>", 0 otherwise. */
uint32_t dhikr_profile_custom_id_number(const char* id);

/**
 * Menu label for the profile list: "* Name" for the active profile and
 * "  Name" otherwise, so names stay aligned.
 */
void dhikr_state_profile_label(const DhikrState* state, uint8_t index, char* out, size_t out_size);

/** Full consistency check of a state, e.g. after loading. */
bool dhikr_state_is_valid(const DhikrState* state);

#ifdef __cplusplus
}
#endif
