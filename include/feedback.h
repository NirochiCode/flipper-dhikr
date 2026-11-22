/**
 * @file feedback.h
 * @brief Feedback modes (hardware independent part).
 *
 * The actual vibration/sound output lives in feedback_player.h so that this
 * header can be used by host unit tests without the Flipper SDK.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Persisted as its numeric value: never reorder, only append. */
typedef enum {
    DhikrFeedbackOff = 0,
    DhikrFeedbackVibro = 1,
    DhikrFeedbackSound = 2,
    DhikrFeedbackBoth = 3,
    DhikrFeedbackModeCount,
} DhikrFeedbackMode;

#define DHIKR_FEEDBACK_DEFAULT DhikrFeedbackVibro

typedef enum {
    /** Short, subtle feedback for a single count. */
    DhikrFeedbackEventCount,
    /** Distinct feedback when the target is reached for the first time. */
    DhikrFeedbackEventTarget,
} DhikrFeedbackEvent;

bool dhikr_feedback_mode_is_valid(uint32_t mode);
bool dhikr_feedback_mode_uses_vibro(DhikrFeedbackMode mode);
bool dhikr_feedback_mode_uses_sound(DhikrFeedbackMode mode);
/** @return short display name, "?" for invalid modes. */
const char* dhikr_feedback_mode_name(DhikrFeedbackMode mode);

#ifdef __cplusplus
}
#endif
