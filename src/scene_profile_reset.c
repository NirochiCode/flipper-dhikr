/*
 * "Reset progress?" for the profile chosen in the details menu
 * (app->edit_profile_id), not necessarily the active one. No is pre-selected;
 * Back and No cancel. Yes sets count and completed sets of that profile to 0,
 * clears its undo, saves at once and returns to its details menu.
 */
#include "include/dhikr_app.h"

#include <stdio.h>
#include <string.h>

static void profile_reset_confirm_callback(DhikrConfirmViewEvent event, void* context) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        event == DhikrConfirmViewEventAccepted ? DhikrEventConfirmAccepted :
                                                 DhikrEventConfirmCancelled);
}

void dhikr_scene_profile_reset_on_enter(void* context) {
    DhikrApp* app = context;
    if(!dhikr_app_resolve_edit_profile(app)) {
        // The profile no longer exists: nothing to reset.
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, DhikrSceneProfiles);
        return;
    }
    const DhikrProfile* profile = &app->state.profiles[app->edit_profile];
    char detail[DHIKR_LABEL_SIZE];
    snprintf(
        detail,
        sizeof(detail),
        "%lu/%lu, sets %lu",
        (unsigned long)profile->counter.count,
        (unsigned long)profile->counter.target,
        (unsigned long)profile->counter.rounds);

    // Names are at most 16 characters and fit the dialog's small font in full.
    // Selection always starts at "No".
    dhikr_confirm_view_setup(app->confirm_view, "Reset progress?", profile->name, detail);
    dhikr_confirm_view_set_callback(app->confirm_view, profile_reset_confirm_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewConfirm);
}

bool dhikr_scene_profile_reset_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == DhikrEventConfirmAccepted) {
        // By id: exactly the chosen profile, never implicitly the active one.
        if(dhikr_state_reset_profile(&app->state, app->edit_profile_id)) {
            dhikr_app_mark_dirty(app);
            dhikr_app_save_if_dirty(app);
        }
        scene_manager_previous_scene(app->scene_manager); // details menu of the same profile
        return true;
    }
    if(event.event == DhikrEventConfirmCancelled) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void dhikr_scene_profile_reset_on_exit(void* context) {
    DhikrApp* app = context;
    dhikr_confirm_view_set_callback(app->confirm_view, NULL, NULL);
}
