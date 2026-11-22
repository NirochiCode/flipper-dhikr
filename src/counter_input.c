#include "include/counter_input.h"

#include <stddef.h>

DhikrCounterAction dhikr_counter_input_map(DhikrKey key, DhikrPress press) {
    if(press == DhikrPressShort) {
        switch(key) {
        case DhikrKeyOk:
            return DhikrCounterActionIncrement;
        case DhikrKeyLeft:
            return DhikrCounterActionPrevProfile;
        case DhikrKeyRight:
            return DhikrCounterActionNextProfile;
        case DhikrKeyBack:
            return DhikrCounterActionExit;
        default:
            return DhikrCounterActionNone;
        }
    }
    // InputTypeLong is sent exactly once per hold; the following Repeat events are ignored.
    if(press == DhikrPressLong && key == DhikrKeyDown) return DhikrCounterActionUndo;
    return DhikrCounterActionNone;
}

static uint32_t banner_duration_ms(DhikrBannerKind kind) {
    switch(kind) {
    case DhikrBannerTarget:
        return DHIKR_BANNER_TARGET_MS;
    case DhikrBannerUndo:
    case DhikrBannerRoundRestored:
    case DhikrBannerNothingToUndo:
        return DHIKR_BANNER_INFO_MS;
    case DhikrBannerNone:
    default:
        return 0;
    }
}

void dhikr_banner_show(DhikrBanner* banner, DhikrBannerKind kind, uint32_t value, uint32_t now_ms) {
    if(banner == NULL) return;
    banner->kind = kind;
    banner->value = value;
    banner->started_ms = now_ms;
}

void dhikr_banner_clear(DhikrBanner* banner) {
    if(banner == NULL) return;
    banner->kind = DhikrBannerNone;
    banner->value = 0;
    banner->started_ms = 0;
}

uint32_t dhikr_banner_remaining_ms(const DhikrBanner* banner, uint32_t now_ms) {
    if(banner == NULL || banner->kind == DhikrBannerNone) return 0;
    uint32_t elapsed = now_ms - banner->started_ms; // unsigned: wrap-around safe
    uint32_t duration = banner_duration_ms(banner->kind);
    return elapsed < duration ? duration - elapsed : 0;
}

bool dhikr_banner_is_visible(const DhikrBanner* banner, uint32_t now_ms) {
    return dhikr_banner_remaining_ms(banner, now_ms) > 0;
}

DhikrActionOutcome dhikr_counter_apply(
    DhikrState* state,
    DhikrBanner* banner,
    DhikrCounterAction action,
    uint32_t now_ms) {
    DhikrActionOutcome outcome = {0};
    DhikrProfile* profile = dhikr_state_active(state);
    if(profile == NULL) return outcome;
    DhikrCounter* counter = &profile->counter;

    switch(action) {
    case DhikrCounterActionIncrement: {
        uint32_t target = counter->target;
        outcome.changed = true;
        outcome.play_feedback = true;
        if(dhikr_counter_increment(counter) == DhikrIncrementRoundCompleted) {
            dhikr_banner_show(banner, DhikrBannerTarget, target, now_ms);
            outcome.feedback_event = DhikrFeedbackEventTarget;
            outcome.save_now = true; // the reset to 0 must reach the SD card
        } else {
            dhikr_banner_clear(banner);
            outcome.feedback_event = DhikrFeedbackEventCount;
        }
        break;
    }
    case DhikrCounterActionUndo:
        switch(dhikr_counter_undo(counter)) {
        case DhikrUndoDecremented:
            dhikr_banner_show(banner, DhikrBannerUndo, counter->count, now_ms);
            outcome.changed = true;
            outcome.save_now = true;
            break;
        case DhikrUndoRoundRestored:
            dhikr_banner_show(banner, DhikrBannerRoundRestored, counter->count, now_ms);
            outcome.changed = true;
            outcome.save_now = true;
            break;
        case DhikrUndoNothing:
        default:
            dhikr_banner_show(banner, DhikrBannerNothingToUndo, 0, now_ms);
            break;
        }
        break;
    case DhikrCounterActionPrevProfile:
    case DhikrCounterActionNextProfile:
        dhikr_state_cycle(state, action == DhikrCounterActionNextProfile ? 1 : -1);
        dhikr_banner_clear(banner);
        outcome.changed = true;
        break;
    case DhikrCounterActionNone:
    case DhikrCounterActionExit:
    default:
        break;
    }
    return outcome;
}
