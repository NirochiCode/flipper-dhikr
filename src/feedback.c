#include "include/feedback.h"

#include <stddef.h>

void dhikr_feedback_settings_init_defaults(DhikrFeedbackSettings* settings) {
    if(settings == NULL) return;
    settings->counter_vibration = false;
    settings->counter_sound = false;
    settings->target_vibration = true;
    settings->target_sound = true;
}

bool dhikr_feedback_event_enabled(const DhikrFeedbackSettings* settings, DhikrFeedbackEvent event) {
    return dhikr_feedback_select(settings, event) != DhikrFeedbackPatternNone;
}

bool dhikr_feedback_any_enabled(const DhikrFeedbackSettings* settings) {
    return dhikr_feedback_event_enabled(settings, DhikrFeedbackEventCount) ||
           dhikr_feedback_event_enabled(settings, DhikrFeedbackEventTarget);
}

DhikrFeedbackPattern
    dhikr_feedback_select(const DhikrFeedbackSettings* settings, DhikrFeedbackEvent event) {
    if(settings == NULL) return DhikrFeedbackPatternNone;

    if(event == DhikrFeedbackEventTarget) {
        if(settings->target_vibration && settings->target_sound) {
            return DhikrFeedbackPatternTargetBoth;
        }
        if(settings->target_vibration) return DhikrFeedbackPatternTargetVibro;
        if(settings->target_sound) return DhikrFeedbackPatternTargetSound;
        return DhikrFeedbackPatternNone;
    }

    if(settings->counter_vibration && settings->counter_sound) return DhikrFeedbackPatternCountBoth;
    if(settings->counter_vibration) return DhikrFeedbackPatternCountVibro;
    if(settings->counter_sound) return DhikrFeedbackPatternCountSound;
    return DhikrFeedbackPatternNone;
}

bool dhikr_feedback_pattern_uses_vibro(DhikrFeedbackPattern pattern) {
    return pattern == DhikrFeedbackPatternCountVibro || pattern == DhikrFeedbackPatternCountBoth ||
           pattern == DhikrFeedbackPatternTargetVibro || pattern == DhikrFeedbackPatternTargetBoth;
}

bool dhikr_feedback_pattern_uses_sound(DhikrFeedbackPattern pattern) {
    return pattern == DhikrFeedbackPatternCountSound || pattern == DhikrFeedbackPatternCountBoth ||
           pattern == DhikrFeedbackPatternTargetSound || pattern == DhikrFeedbackPatternTargetBoth;
}

bool dhikr_feedback_pattern_is_target(DhikrFeedbackPattern pattern) {
    return pattern == DhikrFeedbackPatternTargetVibro ||
           pattern == DhikrFeedbackPatternTargetSound || pattern == DhikrFeedbackPatternTargetBoth;
}

bool dhikr_feedback_legacy_mode_is_valid(uint32_t mode) {
    return mode < (uint32_t)DhikrFeedbackLegacyModeCount;
}

void dhikr_feedback_settings_from_legacy(DhikrFeedbackSettings* settings, DhikrFeedbackLegacyMode mode) {
    if(settings == NULL) return;
    const bool vibro = mode == DhikrFeedbackLegacyVibro || mode == DhikrFeedbackLegacyBoth;
    const bool sound = mode == DhikrFeedbackLegacySound || mode == DhikrFeedbackLegacyBoth;
    settings->counter_vibration = vibro;
    settings->target_vibration = vibro;
    settings->counter_sound = sound;
    settings->target_sound = sound;
}
