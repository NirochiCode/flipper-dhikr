/*
 * Profile details for the profile chosen in the list (app->edit_profile_id),
 * header = profile name:
 *   View Progress      -> read-only: current count, completed sets, target
 *   Edit Target (33)   -> target presets 33 / 99 / custom for THIS profile
 *   Rename Profile     (custom profiles only)
 *   Reset Progress     -> "Reset progress?" (No pre-selected)
 *   Delete Profile     (custom profiles only)
 * Nothing here changes which profile is active.
 */
#include "include/dhikr_app.h"
#include "include/menu.h"

#include <stdio.h>
#include <string.h>

static void profile_actions_callback(void* context, uint32_t index) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dhikr_scene_profile_actions_on_enter(void* context) {
    DhikrApp* app = context;
    Submenu* submenu = app->submenu;
    if(!dhikr_app_resolve_edit_profile(app)) {
        // The profile no longer exists (should not happen): back to the list.
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, DhikrSceneProfiles);
        return;
    }
    const DhikrProfile* profile = &app->state.profiles[app->edit_profile];

    // Local copy: source and destination both live inside `app`.
    char name[DHIKR_PROFILE_NAME_SIZE];
    memcpy(name, profile->name, sizeof(name));

    submenu_reset(submenu);
    snprintf(app->header, sizeof(app->header), "%s", name);
    submenu_set_header(submenu, app->header);

    DhikrProfileAction actions[DhikrProfileActionCount];
    const uint8_t count = dhikr_profile_actions(profile, actions);
    for(uint8_t i = 0; i < count; i++) {
        dhikr_profile_action_label(profile, actions[i], app->labels[i], DHIKR_LABEL_SIZE);
        submenu_add_item(submenu, app->labels[i], actions[i], profile_actions_callback, app);
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, DhikrSceneProfileActions));
    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewSubmenu);
}

bool dhikr_scene_profile_actions_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfileActions, 0);
        return false;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event >= DhikrProfileActionCount) return false;

    const DhikrProfileAction action = (DhikrProfileAction)event.event;
    if(!dhikr_profile_action_allowed(&app->state.profiles[app->edit_profile], action)) {
        return true;
    }
    scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfileActions, action);
    switch(action) {
    case DhikrProfileActionProgress:
        scene_manager_next_scene(app->scene_manager, DhikrSceneProfileProgress);
        return true;
    case DhikrProfileActionTarget:
        scene_manager_next_scene(app->scene_manager, DhikrSceneTarget);
        return true;
    case DhikrProfileActionRename:
        app->name_mode = DhikrNameModeRename;
        scene_manager_next_scene(app->scene_manager, DhikrSceneProfileName);
        return true;
    case DhikrProfileActionReset:
        scene_manager_next_scene(app->scene_manager, DhikrSceneProfileReset);
        return true;
    case DhikrProfileActionDelete:
        scene_manager_next_scene(app->scene_manager, DhikrSceneProfileDelete);
        return true;
    case DhikrProfileActionCount:
    default:
        return false;
    }
}

void dhikr_scene_profile_actions_on_exit(void* context) {
    DhikrApp* app = context;
    submenu_reset(app->submenu);
}
