/**
 * @file feedback_player.h
 * @brief Plays feedback through the Flipper notification service.
 *
 * Sequences are queued to the notification service and played on its own
 * thread, so the UI never blocks. Every sequence ends by switching the
 * speaker/vibration motor off, so nothing keeps running. System-wide
 * notification settings (volume, vibration on/off, stealth mode) are
 * respected because no "force" messages are used.
 */
#pragma once

#include <notification/notification.h>

#include "include/feedback.h"

void dhikr_feedback_play(
    NotificationApp* notifications,
    DhikrFeedbackMode mode,
    DhikrFeedbackEvent event);
