/**
 * @file counter.h
 * @brief Hardware-independent tasbih counter logic.
 *
 * Since v1.0.1 the counter works in rounds: the increment that reaches the
 * target completes a round. The round counter goes up by one and the count
 * goes back to 0 immediately, so the stored state is always consistent, even
 * if the app is closed while "Target reached!" is still on screen.
 *
 * Undo takes back the last count step. If that step completed a round, undo
 * restores the exact count from before it and removes the round again. This
 * only works directly after the completion (see `undo_round_available`).
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
/**
 * Upper bound for a stored count. With rounds the count normally stays below
 * the target. Larger values can only come from v1.0 data or from lowering the
 * target, and the next count then completes a round.
 */
#define DHIKR_COUNT_MAX 999999UL
/** Completed rounds saturate here (fits the display). */
#define DHIKR_ROUNDS_MAX 999999UL

typedef struct {
    uint32_t count;
    uint32_t target;
    /** Completed rounds; persisted, separate from the current count. */
    uint32_t rounds;
    /* --- Not persisted: undo information for the last completed round. --- */
    bool undo_round_available;
    uint32_t count_before_completion;
} DhikrCounter;

typedef enum {
    /** Count increased by one, target not reached. */
    DhikrIncrementCounted,
    /** This count reached the target: rounds + 1, count reset to 0. */
    DhikrIncrementRoundCompleted,
} DhikrIncrementResult;

typedef enum {
    /** Count decreased by exactly one. */
    DhikrUndoDecremented,
    /** The round completed by the last count was taken back. */
    DhikrUndoRoundRestored,
    /** Count is 0 and no round can be restored: nothing changed. */
    DhikrUndoNothing,
} DhikrUndoResult;

/** Initialise with count 0 and rounds 0. An invalid target falls back to the default. */
void dhikr_counter_init(DhikrCounter* counter, uint32_t target);

/** Count one. Completes a round (and resets the count) when the target is reached. */
DhikrIncrementResult dhikr_counter_increment(DhikrCounter* counter);

/** Undo the last count step; never goes below 0. */
DhikrUndoResult dhikr_counter_undo(DhikrCounter* counter);

/** Set count and rounds back to 0. The target is kept. */
void dhikr_counter_reset(DhikrCounter* counter);

/** @return true if `target` is within [DHIKR_TARGET_MIN, DHIKR_TARGET_MAX]. */
bool dhikr_target_is_valid(uint32_t target);

/**
 * Change the target without touching count or rounds. If the count is
 * already >= the new target, the next count completes the round.
 * @return false (and leave the counter unchanged) if the target is invalid.
 */
bool dhikr_counter_set_target(DhikrCounter* counter, uint32_t target);

/** @return progress towards the target in 0..1000 (clamped). */
uint32_t dhikr_counter_progress_permille(const DhikrCounter* counter);

/** @return true if count, target and rounds are within their valid ranges. */
bool dhikr_counter_is_valid(const DhikrCounter* counter);

/**
 * Fold a count that ran past the target (allowed in v1.0) into completed
 * rounds without losing anything: rounds += count / target, count %= target.
 */
void dhikr_counter_normalize(DhikrCounter* counter);

#ifdef __cplusplus
}
#endif
