#include "include/counter.h"

#include <stddef.h>

static void counter_clear_undo(DhikrCounter* counter) {
    counter->undo_round_available = false;
    counter->count_before_completion = 0;
}

void dhikr_counter_init(DhikrCounter* counter, uint32_t target) {
    if(counter == NULL) return;
    counter->count = 0;
    counter->rounds = 0;
    counter->target = dhikr_target_is_valid(target) ? target : DHIKR_TARGET_DEFAULT;
    counter_clear_undo(counter);
}

DhikrIncrementResult dhikr_counter_increment(DhikrCounter* counter) {
    if(counter == NULL) return DhikrIncrementCounted;

    // count + 1 >= target, written so that it cannot overflow.
    if(counter->count >= counter->target || counter->target - counter->count <= 1) {
        counter->count_before_completion = counter->count;
        counter->undo_round_available = true;
        counter->count = 0;
        if(counter->rounds < DHIKR_ROUNDS_MAX) counter->rounds++;
        return DhikrIncrementRoundCompleted;
    }

    counter->count++;
    counter_clear_undo(counter);
    return DhikrIncrementCounted;
}

DhikrUndoResult dhikr_counter_undo(DhikrCounter* counter) {
    if(counter == NULL) return DhikrUndoNothing;

    if(counter->count == 0 && counter->undo_round_available && counter->rounds > 0) {
        counter->count = counter->count_before_completion;
        counter->rounds--;
        counter_clear_undo(counter);
        return DhikrUndoRoundRestored;
    }

    counter_clear_undo(counter);
    if(counter->count == 0) return DhikrUndoNothing;
    counter->count--;
    return DhikrUndoDecremented;
}

void dhikr_counter_reset(DhikrCounter* counter) {
    if(counter == NULL) return;
    counter->count = 0;
    counter->rounds = 0;
    counter_clear_undo(counter);
}

bool dhikr_target_is_valid(uint32_t target) {
    return target >= DHIKR_TARGET_MIN && target <= DHIKR_TARGET_MAX;
}

bool dhikr_counter_set_target(DhikrCounter* counter, uint32_t target) {
    if(counter == NULL || !dhikr_target_is_valid(target)) return false;
    counter->target = target;
    // The restored count would refer to the old target.
    counter_clear_undo(counter);
    return true;
}

uint32_t dhikr_counter_progress_permille(const DhikrCounter* counter) {
    if(counter == NULL || counter->target == 0) return 0;
    if(counter->count >= counter->target) return 1000;
    // count < target <= 9999, so count * 1000 cannot overflow uint32_t.
    return (counter->count * 1000UL) / counter->target;
}

bool dhikr_counter_is_valid(const DhikrCounter* counter) {
    return counter != NULL && counter->count <= DHIKR_COUNT_MAX &&
           counter->rounds <= DHIKR_ROUNDS_MAX && dhikr_target_is_valid(counter->target);
}

void dhikr_counter_normalize(DhikrCounter* counter) {
    if(counter == NULL || !dhikr_target_is_valid(counter->target)) return;
    if(counter->count < counter->target) return;

    uint32_t extra_rounds = counter->count / counter->target;
    counter->count %= counter->target;
    uint32_t rounds = counter->rounds + extra_rounds; // both <= 999999: no overflow
    counter->rounds = rounds > DHIKR_ROUNDS_MAX ? DHIKR_ROUNDS_MAX : rounds;
    counter_clear_undo(counter);
}
