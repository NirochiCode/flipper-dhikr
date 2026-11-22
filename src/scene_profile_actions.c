/* Edit menu for one profile (app->edit_profile): target, rename, delete. */
#include "include/dhikr_app.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    ActionTarget,
    ActionRename,
    ActionDelete,
} ProfileAction;

static void profile_actions_callback(void* context, uint32_t index) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dhikr_scene_profile_actions_on_enter(void* context) {
    DhikrApp* app = context;
    Submenu* submenu = app->submenu;
    if(app->edit_profile >= app->state.profile_count) app->edit_profile = 0;
    const DhikrProfile* profile = &app->state.profiles[app->edit_profile];

    // Local copy: source and destination both live inside `app`.
    char name[DHIKR_PROFILE_NAME_SIZE];
    memcpy(name, profile->name, sizeof(name));

    submenu_reset(submenu);
    snprintf(app->header, sizeof(app->header), "%s", name);
    submenu_set_header(submenu, app->header);

    snprintf(
        app->labels[0],
        DHIKR_LABEL_SIZE,
        "Target: %lu",
        (unsigned long)profile->counter.target);
    submenu_add_item(submenu, app->labels[0], ActionTarget, profile_actions_callback, app);
    if(!profile->builtin) {
        // Built-in profiles keep their names and cannot be deleted.
        submenu_add_item(submenu, "Rename", ActionRename, profile_actions_callback, app);
        submenu_add_item(submenu, "Delete", ActionDelete, profile_actions_callback, app);
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

    const bool builtin = app->state.profiles[app->edit_profile].builtin;
    switch(event.event) {
    case ActionTarget:
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfileActions, ActionTarget);
        app->target_mode = DhikrTargetModeEditProfile;
        scene_manager_next_scene(app->scene_manager, DhikrSceneTargetCustom);
        return true;
    case ActionRename:
        if(builtin) return true;
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfileActions, ActionRename);
        app->name_mode = DhikrNameModeRename;
        scene_manager_next_scene(app->scene_manager, DhikrSceneProfileName);
        return true;
    case ActionDelete:
        if(builtin) return true;
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfileActions, ActionDelete);
        scene_manager_next_scene(app->scene_manager, DhikrSceneProfileDelete);
        return true;
    default:
        return false;
    }
}

void dhikr_scene_profile_actions_on_exit(void* context) {
    DhikrApp* app = context;
    submenu_reset(app->submenu);
}
