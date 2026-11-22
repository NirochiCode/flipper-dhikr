/**
 * @file feedback.h
 * @brief Feedback settings and pattern selection (hardware independent part).
 *
 * Since v1.0.2 feedback is configured with four independent switches:
 * vibration and sound for a normal count, and vibration and sound for a
 * completed target. The actual output lives in feedback_player.h so that this
 * header can be used by host unit tests without the Flipper SDK.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool counter_vibration;
    bool counter_sound;
    bool target_vibration;
    bool target_sound;
} DhikrFeedbackSettings;

typedef enum {
    /** Short, subtle feedback for a single count. */
    DhikrFeedbackEventCount,
    /** Distinct feedback when a round is completed. */
    DhikrFeedbackEventTarget,
} DhikrFeedbackEvent;

/** Exactly one pattern is played per event (or none). */
typedef enum {
    DhikrFeedbackPatternNone,
    DhikrFeedbackPatternCountVibro,
    DhikrFeedbackPatternCountSound,
    DhikrFeedbackPatternCountBoth,
    /* Target patterns are longer, clearly different, and blink the green LED. */
    DhikrFeedbackPatternTargetVibro,
    DhikrFeedbackPatternTargetSound,
    DhikrFeedbackPatternTargetBoth,
} DhikrFeedbackPattern;

/**
 * Defaults for a fresh install: quiet counting, vibration + sound when a
 * target is reached.
 */
void dhikr_feedback_settings_init_defaults(DhikrFeedbackSettings* settings);

/** @return true if at least one output is enabled for `event`. */
bool dhikr_feedback_event_enabled(const DhikrFeedbackSettings* settings, DhikrFeedbackEvent event);

/** @return true if any of the four switches is on. */
bool dhikr_feedback_any_enabled(const DhikrFeedbackSettings* settings);

/**
 * Choose the single pattern for an event. A count event only looks at the
 * counter switches and a target event only at the target switches, so a
 * completed target never adds the counter pattern on top.
 */
DhikrFeedbackPattern
    dhikr_feedback_select(const DhikrFeedbackSettings* settings, DhikrFeedbackEvent event);

bool dhikr_feedback_pattern_uses_vibro(DhikrFeedbackPattern pattern);
bool dhikr_feedback_pattern_uses_sound(DhikrFeedbackPattern pattern);
bool dhikr_feedback_pattern_is_target(DhikrFeedbackPattern pattern);

/* --- Legacy single mode (settings format v1/v2, app v1.0 / v1.0.1) ------------ */

/** Persisted as its numeric value in format v1/v2. */
typedef enum {
    DhikrFeedbackLegacyOff = 0,
    DhikrFeedbackLegacyVibro = 1,
    DhikrFeedbackLegacySound = 2,
    DhikrFeedbackLegacyBoth = 3,
    DhikrFeedbackLegacyModeCount,
} DhikrFeedbackLegacyMode;

bool dhikr_feedback_legacy_mode_is_valid(uint32_t mode);

/**
 * Map an old mode to the four switches so existing users keep the same
 * behaviour: Off -> all off, Vibration -> both vibration switches,
 * Sound -> both sound switches, Both -> all on.
 */
void dhikr_feedback_settings_from_legacy(DhikrFeedbackSettings* settings, DhikrFeedbackLegacyMode mode);

#ifdef __cplusplus
}
#endif
