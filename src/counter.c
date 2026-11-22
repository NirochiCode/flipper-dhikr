#include "include/counter.h"

#include <stddef.h>

void dhikr_counter_init(DhikrCounter* counter, uint32_t target) {
    if(counter == NULL) return;
    counter->count = 0;
    counter->target = dhikr_target_is_valid(target) ? target : DHIKR_TARGET_DEFAULT;
}

DhikrIncrementResult dhikr_counter_increment(DhikrCounter* counter) {
    if(counter == NULL || counter->count >= DHIKR_COUNT_MAX) {
        return DhikrIncrementSaturated;
    }

    const bool was_complete = dhikr_counter_is_complete(counter);
    counter->count++;

    if(!was_complete && dhikr_counter_is_complete(counter)) {
        return DhikrIncrementTargetReached;
    }
    return DhikrIncrementCounted;
}

void dhikr_counter_reset(DhikrCounter* counter) {
    if(counter == NULL) return;
    counter->count = 0;
}

bool dhikr_target_is_valid(uint32_t target) {
    return target >= DHIKR_TARGET_MIN && target <= DHIKR_TARGET_MAX;
}

bool dhikr_counter_set_target(DhikrCounter* counter, uint32_t target) {
    if(counter == NULL || !dhikr_target_is_valid(target)) return false;
    counter->target = target;
    return true;
}

bool dhikr_counter_is_complete(const DhikrCounter* counter) {
    return counter != NULL && counter->count >= counter->target;
}

bool dhikr_counter_is_saturated(const DhikrCounter* counter) {
    return counter != NULL && counter->count >= DHIKR_COUNT_MAX;
}

uint32_t dhikr_counter_progress_permille(const DhikrCounter* counter) {
    if(counter == NULL || counter->target == 0) return 0;
    if(counter->count >= counter->target) return 1000;
    // count < target <= 9999, so count * 1000 cannot overflow uint32_t.
    return (counter->count * 1000UL) / counter->target;
}

bool dhikr_counter_is_valid(const DhikrCounter* counter) {
    return counter != NULL && counter->count <= DHIKR_COUNT_MAX &&
           dhikr_target_is_valid(counter->target);
}
