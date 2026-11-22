/**
 * @file feedback_player.h
 * @brief Plays feedback through the Flipper notification service.
 *
 * Sequences are queued to the notification service and played on its own
 * thread, so the UI never blocks. Every sequence ends by switching the
 * speaker/vibration motor off, so nothing keeps running.
 *
 * System settings are respected on purpose (no "force" messages): the
 * notification service skips vibration if Settings > Notifications > Vibro
 * is off, plays sound at the system volume (silent at 0 %) and skips both
 * while stealth mode is on.
 */
#pragma once

#include <notification/notification.h>

#include "include/feedback.h"

/**
 * Play the single pattern chosen by dhikr_feedback_select() for this event.
 * @return true if a sequence was queued (false: all switches for the event are off)
 */
bool dhikr_feedback_play(
    NotificationApp* notifications,
    const DhikrFeedbackSettings* settings,
    DhikrFeedbackEvent event);

/**
 * @return true if stealth mode is on, which mutes all vibration and sound
 *         system-wide. (The Vibro switch and the volume cannot be read
 *         through the public SDK, so they are only mentioned in the help.)
 */
bool dhikr_feedback_system_muted(void);
