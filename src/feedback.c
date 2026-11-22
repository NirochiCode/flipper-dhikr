#include "include/feedback.h"

static const char* const feedback_mode_names[DhikrFeedbackModeCount] = {
    [DhikrFeedbackOff] = "Off",
    [DhikrFeedbackVibro] = "Vibration",
    [DhikrFeedbackSound] = "Sound",
    [DhikrFeedbackBoth] = "Both",
};

bool dhikr_feedback_mode_is_valid(uint32_t mode) {
    return mode < (uint32_t)DhikrFeedbackModeCount;
}

bool dhikr_feedback_mode_uses_vibro(DhikrFeedbackMode mode) {
    return mode == DhikrFeedbackVibro || mode == DhikrFeedbackBoth;
}

bool dhikr_feedback_mode_uses_sound(DhikrFeedbackMode mode) {
    return mode == DhikrFeedbackSound || mode == DhikrFeedbackBoth;
}

const char* dhikr_feedback_mode_name(DhikrFeedbackMode mode) {
    if(!dhikr_feedback_mode_is_valid((uint32_t)mode)) return "?";
    return feedback_mode_names[mode];
}
