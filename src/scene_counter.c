#include "include/dhikr_app.h"
#include "include/feedback_player.h"

static void counter_view_callback(DhikrCounterViewEvent event, void* context) {
    DhikrApp* app = context;
    static const uint32_t events[] = {
        [DhikrCounterViewEventIncrement] = DhikrEventCounterIncrement,
        [DhikrCounterViewEventPrevProfile] = DhikrEventCounterPrev,
        [DhikrCounterViewEventNextProfile] = DhikrEventCounterNext,
    };
    view_dispatcher_send_custom_event(app->view_dispatcher, events[event]);
}

static void counter_scene_refresh(DhikrApp* app) {
    dhikr_counter_view_update(
        app->counter_view, dhikr_state_active(&app->state), app->state.profile_count);
}

void dhikr_scene_counter_on_enter(void* context) {
    DhikrApp* app = context;
    dhikr_counter_view_set_callback(app->counter_view, counter_view_callback, app);
    counter_scene_refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewCounter);
}

bool dhikr_scene_counter_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case DhikrEventCounterIncrement: {
        DhikrProfile* profile = dhikr_state_active(&app->state);
        DhikrIncrementResult result = dhikr_counter_increment(&profile->counter);
        if(result != DhikrIncrementSaturated) {
            dhikr_app_mark_dirty(app);
            dhikr_feedback_play(
                app->notifications,
                app->state.feedback,
                result == DhikrIncrementTargetReached ? DhikrFeedbackEventTarget :
                                                        DhikrFeedbackEventCount);
        }
        counter_scene_refresh(app);
        // The tick callback does not fire while keys are pressed continuously.
        dhikr_app_autosave_tick(app);
        return true;
    }
    case DhikrEventCounterPrev:
    case DhikrEventCounterNext:
        dhikr_state_cycle(&app->state, event.event == DhikrEventCounterNext ? 1 : -1);
        dhikr_app_mark_dirty(app);
        counter_scene_refresh(app);
        return true;
    default:
        return false;
    }
}

void dhikr_scene_counter_on_exit(void* context) {
    DhikrApp* app = context;
    dhikr_counter_view_set_callback(app->counter_view, NULL, NULL);
    dhikr_app_save_if_dirty(app);
}
