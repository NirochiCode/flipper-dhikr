#include "include/feedback_player.h"

#include <furi_hal_rtc.h>
#include <notification/notification_messages.h>

/*
 * Durations: count ~50 ms, target ~0.5 s. The target pattern is shorter than
 * the 1 s "Target reached!" banner, so it has finished before normal
 * counting resumes and cannot pile up in the notification queue (which is
 * shared with the input service and blocks the sender when full).
 */

/* --- Count: short, subtle ---------------------------------------------------- */

static const NotificationSequence sequence_count_vibro = {
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence sequence_count_sound = {
    &message_note_c7,
    &message_delay_25,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_count_both = {
    &message_vibro_on,
    &message_note_c7,
    &message_delay_25,
    &message_sound_off,
    &message_delay_25,
    &message_vibro_off,
    NULL,
};

/* --- Target reached: two long pulses / rising arpeggio, green LED ------------ */

static const NotificationSequence sequence_target_vibro = {
    &message_green_255,
    &message_vibro_on,
    &message_delay_250,
    &message_vibro_off,
    &message_delay_100,
    &message_vibro_on,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_green_0,
    NULL,
};

static const NotificationSequence sequence_target_sound = {
    &message_green_255,
    &message_note_c6,
    &message_delay_100,
    &message_note_e6,
    &message_delay_100,
    &message_note_g6,
    &message_delay_100,
    &message_note_c7,
    &message_delay_250,
    &message_sound_off,
    &message_green_0,
    NULL,
};

static const NotificationSequence sequence_target_both = {
    &message_green_255,
    &message_vibro_on,
    &message_note_c6,
    &message_delay_100,
    &message_note_e6,
    &message_delay_100,
    &message_vibro_off,
    &message_note_g6,
    &message_delay_100,
    &message_vibro_on,
    &message_note_c7,
    &message_delay_250,
    &message_sound_off,
    &message_vibro_off,
    &message_green_0,
    NULL,
};

bool dhikr_feedback_play(
    NotificationApp* notifications,
    const DhikrFeedbackSettings* settings,
    DhikrFeedbackEvent event) {
    if(notifications == NULL) return false;

    // NotificationSequence is an array type, so a switch is used instead of a table of ?:
    const NotificationSequence* sequence = NULL;
    switch(dhikr_feedback_select(settings, event)) {
    case DhikrFeedbackPatternCountVibro:
        sequence = &sequence_count_vibro;
        break;
    case DhikrFeedbackPatternCountSound:
        sequence = &sequence_count_sound;
        break;
    case DhikrFeedbackPatternCountBoth:
        sequence = &sequence_count_both;
        break;
    case DhikrFeedbackPatternTargetVibro:
        sequence = &sequence_target_vibro;
        break;
    case DhikrFeedbackPatternTargetSound:
        sequence = &sequence_target_sound;
        break;
    case DhikrFeedbackPatternTargetBoth:
        sequence = &sequence_target_both;
        break;
    case DhikrFeedbackPatternNone:
    default:
        break;
    }

    if(sequence == NULL) return false;
    notification_message(notifications, sequence);
    return true;
}

bool dhikr_feedback_system_muted(void) {
    return furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode);
}
