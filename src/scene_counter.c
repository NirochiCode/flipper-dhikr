#include "include/dhikr_app.h"
#include "include/feedback_player.h"

static void counter_view_callback(DhikrCounterAction action, void* context) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DhikrEventCounterActionBase + action);
}

static void counter_scene_refresh(DhikrApp* app) {
    const bool muted = dhikr_feedback_any_enabled(&app->state.feedback) &&
                       dhikr_feedback_system_muted();
    dhikr_counter_view_update(
        app->counter_view,
        dhikr_state_active(&app->state),
        app->state.profile_count,
        &app->banner,
        muted);
}

void dhikr_scene_counter_on_enter(void* context) {
    DhikrApp* app = context;
    dhikr_banner_clear(&app->banner);
    app->target_feedback_until_ms = dhikr_ui_now_ms();
    dhikr_counter_view_set_callback(app->counter_view, counter_view_callback, app);
    counter_scene_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewCounter);
}

bool dhikr_scene_counter_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event < DhikrEventCounterActionBase ||
       event.event > DhikrEventCounterActionBase + DhikrCounterActionExit) {
        return false;
    }

    DhikrCounterAction action = (DhikrCounterAction)(event.event - DhikrEventCounterActionBase);
    const uint32_t now = dhikr_ui_now_ms();
    DhikrActionOutcome outcome = dhikr_counter_apply(&app->state, &app->banner, action, now);

    // While the target pattern (~0.6 s) is still playing, further feedback is skipped:
    // the notification queue is shared with the input service and blocks the sender
    // when full. Counts are never skipped, and the banner is always shown.
    const bool feedback_busy = (int32_t)(now - app->target_feedback_until_ms) < 0;
    // Exactly one pattern per action: a completed target plays only the target pattern
    // (or nothing if both target switches are off), never the counter pattern as well.
    if(outcome.play_feedback && !feedback_busy) {
        bool played =
            dhikr_feedback_play(app->notifications, &app->state.feedback, outcome.feedback_event);
        if(played && outcome.feedback_event == DhikrFeedbackEventTarget) {
            app->target_feedback_until_ms = now + DHIKR_TARGET_FEEDBACK_MS;
        }
    }
    counter_scene_refresh(app);

    if(outcome.changed) dhikr_app_mark_dirty(app);
    if(outcome.save_now) {
        // Completed round (count reset to 0) or undo: persist right away (rate-limited).
        dhikr_app_save_soon(app);
    } else {
        // The tick callback does not fire while keys are pressed continuously.
        dhikr_app_autosave_tick(app);
    }
    return true;
}

void dhikr_scene_counter_on_exit(void* context) {
    DhikrApp* app = context;
    dhikr_counter_view_set_callback(app->counter_view, NULL, NULL);
    dhikr_banner_clear(&app->banner);
    dhikr_app_save_if_dirty(app);
}
