/* "Delete profile?" confirmation for custom profiles (No is pre-selected). */
#include "include/dhikr_app.h"

#include <stdio.h>

static void profile_delete_confirm_callback(DhikrConfirmViewEvent event, void* context) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        event == DhikrConfirmViewEventAccepted ? DhikrEventConfirmAccepted :
                                                 DhikrEventConfirmCancelled);
}

void dhikr_scene_profile_delete_on_enter(void* context) {
    DhikrApp* app = context;
    const DhikrProfile* profile = &app->state.profiles[app->edit_profile];
    char detail[DHIKR_LABEL_SIZE];
    snprintf(
        detail,
        sizeof(detail),
        "Count %lu, sets %lu",
        (unsigned long)profile->counter.count,
        (unsigned long)profile->counter.rounds);

    dhikr_confirm_view_setup(app->confirm_view, "Delete profile?", profile->name, detail);
    dhikr_confirm_view_set_callback(app->confirm_view, profile_delete_confirm_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewConfirm);
}

bool dhikr_scene_profile_delete_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == DhikrEventConfirmAccepted) {
        if(dhikr_state_delete(&app->state, app->edit_profile) == DhikrProfileOk) {
            dhikr_app_mark_dirty(app);
            dhikr_app_save_if_dirty(app);
        }
        // The profile is gone: return to the list it was chosen from.
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfileActions, 0);
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfileEdit, 0);
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfiles, 0);
        if(scene_manager_has_previous_scene(app->scene_manager, DhikrSceneProfileEdit)) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, DhikrSceneProfileEdit);
        } else {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, DhikrSceneProfiles);
        }
        return true;
    }
    if(event.event == DhikrEventConfirmCancelled) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void dhikr_scene_profile_delete_on_exit(void* context) {
    DhikrApp* app = context;
    dhikr_confirm_view_set_callback(app->confirm_view, NULL, NULL);
}
