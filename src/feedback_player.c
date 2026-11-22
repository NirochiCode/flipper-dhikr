#include "include/feedback_player.h"

#include <notification/notification_messages.h>

/*
 * All sequences are short (count: ~50 ms, target: ~0.5 s) and explicitly
 * end with sound/vibration off. The notification service plays them on its
 * own thread, so notification_message() returns immediately.
 */

/* --- Count: short, subtle --------------------------------------------------- */

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

/* --- Target reached: longer, distinct pattern ------------------------------- */

static const NotificationSequence sequence_target_vibro = {
    &message_green_255,
    &message_vibro_on,
    &message_delay_250,
    &message_vibro_off,
    &message_delay_100,
    &message_vibro_on,
    &message_delay_250,
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

    const NotificationSequence* sequence = NULL;
    const bool target = event == DhikrFeedbackEventTarget;

    // NotificationSequence is an array type, so plain if/else is used instead of ?:
    switch(mode) {
    case DhikrFeedbackVibro:
        if(target) {
            sequence = &sequence_target_vibro;
        } else {
            sequence = &sequence_count_vibro;
        }
        break;
    case DhikrFeedbackSound:
        if(target) {
            sequence = &sequence_target_sound;
        } else {
            sequence = &sequence_count_sound;
        }
        break;
    case DhikrFeedbackBoth:
        if(target) {
            sequence = &sequence_target_both;
        } else {
            sequence = &sequence_count_both;
        }
        break;
    case DhikrFeedbackOff:
    default:
        break;
    }

    if(sequence != NULL) notification_message(notifications, sequence);
}
