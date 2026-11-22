/**
 * @file counter.h
 * @brief Hardware-independent tasbih counter logic.
 *
 * A counter holds the current count and a target. The "target reached" state
 * is derived (count >= target), so it never needs separate persistence and
 * cannot get out of sync with the stored values.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Smallest accepted target value. */
#define DHIKR_TARGET_MIN 1UL
/** Largest accepted target value. */
#define DHIKR_TARGET_MAX 9999UL
/** Preset target offered in the target menu. */
#define DHIKR_TARGET_PRESET_A 33UL
/** Preset target offered in the target menu. */
#define DHIKR_TARGET_PRESET_B 99UL
/** Default target for new profiles. */
#define DHIKR_TARGET_DEFAULT DHIKR_TARGET_PRESET_A
/** The counter saturates here instead of overflowing (fits the display). */
#define DHIKR_COUNT_MAX 999999UL

typedef struct {
    uint32_t count;
    uint32_t target;
} DhikrCounter;

typedef enum {
    /** Count increased by one, target not crossed. */
    DhikrIncrementCounted,
    /** Count increased by one and reached the target for the first time. */
    DhikrIncrementTargetReached,
    /** Count is already at DHIKR_COUNT_MAX; nothing changed. */
    DhikrIncrementSaturated,
} DhikrIncrementResult;

/** Initialise with count 0. An invalid target falls back to DHIKR_TARGET_DEFAULT. */
void dhikr_counter_init(DhikrCounter* counter, uint32_t target);

/** Increase the count by exactly one (saturating at DHIKR_COUNT_MAX). */
DhikrIncrementResult dhikr_counter_increment(DhikrCounter* counter);

/** Set the count back to 0. The target is kept. */
void dhikr_counter_reset(DhikrCounter* counter);

/** @return true if `target` is within [DHIKR_TARGET_MIN, DHIKR_TARGET_MAX]. */
bool dhikr_target_is_valid(uint32_t target);

/**
 * Change the target without touching the count.
 * @return false (and leave the counter unchanged) if the target is invalid.
 */
bool dhikr_counter_set_target(DhikrCounter* counter, uint32_t target);

/** @return true once count >= target. */
bool dhikr_counter_is_complete(const DhikrCounter* counter);

/** @return true if the count is at its maximum value. */
bool dhikr_counter_is_saturated(const DhikrCounter* counter);

/** @return progress towards the target in 0..1000 (clamped). */
uint32_t dhikr_counter_progress_permille(const DhikrCounter* counter);

/** @return true if count and target are within their valid ranges. */
bool dhikr_counter_is_valid(const DhikrCounter* counter);

#ifdef __cplusplus
}
#endif
