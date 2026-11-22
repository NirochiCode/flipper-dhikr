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

void dhikr_feedback_play(
    NotificationApp* notifications,
    DhikrFeedbackMode mode,
    DhikrFeedbackEvent event) {
    if(notifications == NULL) return;

    const DhikrFeedbackPlan plan = dhikr_feedback_plan(mode, event);
    const NotificationSequence* sequence = NULL;

    // NotificationSequence is an array type, so plain if/else is used instead of ?:
    if(plan.vibro && plan.sound) {
        if(plan.distinct) {
            sequence = &sequence_target_both;
        } else {
            sequence = &sequence_count_both;
        }
    } else if(plan.vibro) {
        if(plan.distinct) {
            sequence = &sequence_target_vibro;
        } else {
            sequence = &sequence_count_vibro;
        }
    } else if(plan.sound) {
        if(plan.distinct) {
            sequence = &sequence_target_sound;
        } else {
            sequence = &sequence_count_sound;
        }
    }

    if(sequence != NULL) notification_message(notifications, sequence);
}

bool dhikr_feedback_system_muted(void) {
    return furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode);
}
