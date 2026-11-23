#include "include/dhikr_app.h"

#include <stdio.h>

static void reset_confirm_callback(DhikrConfirmViewEvent event, void* context) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        event == DhikrConfirmViewEventAccepted ? DhikrEventConfirmAccepted :
                                                 DhikrEventConfirmCancelled);
}

void dhikr_scene_reset_on_enter(void* context) {
    DhikrApp* app = context;
    const DhikrProfile* profile = dhikr_state_active(&app->state);
    char count_line[DHIKR_LABEL_SIZE];
    snprintf(
        count_line,
        sizeof(count_line),
        "Count %lu, sets %lu",
        (unsigned long)profile->counter.count,
        (unsigned long)profile->counter.rounds);

    // Selection always starts at "No".
    dhikr_confirm_view_setup(app->confirm_view, "Reset counter?", profile->name, count_line);
    dhikr_confirm_view_set_callback(app->confirm_view, reset_confirm_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewConfirm);
}

bool dhikr_scene_reset_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == DhikrEventConfirmAccepted) {
        // Only the active profile is reset (count and sets); persist the new value immediately.
        dhikr_state_reset_active(&app->state);
        dhikr_app_mark_dirty(app);
        dhikr_app_save_if_dirty(app);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(event.event == DhikrEventConfirmCancelled) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void dhikr_scene_reset_on_exit(void* context) {
    DhikrApp* app = context;
    dhikr_confirm_view_set_callback(app->confirm_view, NULL, NULL);
}
