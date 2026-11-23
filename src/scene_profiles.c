/*
 * Profile list.
 *   OK (short) on a profile  -> make it active; the list stays open and the
 *                               "*" marker moves immediately (saved at once)
 *   OK (hold) on a profile   -> details of that profile (progress, target, rename,
 *                               reset, delete); it does not become active
 *   "+ Add Profile"          -> name, then target, then back here
 *   "Edit Profiles..."       -> pick a profile, then the same details menu
 *   Back                     -> main menu
 */
#include "include/dhikr_app.h"

#define ITEM_ADD          100
#define ITEM_EDIT         101
#define EVENT_LONG_OFFSET 200 /* long press on profile i -> EVENT_LONG_OFFSET + i */

static void profiles_item_callback(void* context, InputType input_type, uint32_t index) {
    DhikrApp* app = context;
    if(input_type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, index);
    } else if(input_type == InputTypeLong) {
        view_dispatcher_send_custom_event(app->view_dispatcher, EVENT_LONG_OFFSET + index);
    }
}

static void profiles_menu_callback(void* context, uint32_t index) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void profiles_update_labels(DhikrApp* app) {
    for(uint8_t i = 0; i < app->state.profile_count; i++) {
        dhikr_state_profile_label(&app->state, i, app->labels[i], DHIKR_LABEL_SIZE);
        submenu_change_item_label(app->submenu, i, app->labels[i]);
    }
}

void dhikr_scene_profiles_on_enter(void* context) {
    DhikrApp* app = context;
    Submenu* submenu = app->submenu;
    const DhikrState* state = &app->state;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Dhikr Profiles");

    for(uint8_t i = 0; i < state->profile_count; i++) {
        dhikr_state_profile_label(state, i, app->labels[i], DHIKR_LABEL_SIZE);
        submenu_add_item_ex(submenu, app->labels[i], i, profiles_item_callback, app);
    }
    submenu_add_item(submenu, "+ Add Profile", ITEM_ADD, profiles_menu_callback, app);
    submenu_add_item(submenu, "Edit Profiles...", ITEM_EDIT, profiles_menu_callback, app);

    // Keep the highlighted row when coming back from a sub-screen.
    uint32_t selected = scene_manager_get_scene_state(app->scene_manager, DhikrSceneProfiles);
    if(selected == 0 && state->active != 0) selected = state->active;
    submenu_set_selected_item(submenu, selected);

    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewSubmenu);
}

bool dhikr_scene_profiles_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        // Next time the list opens on the active profile.
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfiles, 0);
        return false;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    const uint32_t count = app->state.profile_count;

    if(event.event < count) {
        // Select and stay in the list.
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfiles, event.event);
        if(dhikr_state_select(&app->state, (uint8_t)event.event)) {
            dhikr_app_mark_dirty(app);
            dhikr_app_save_if_dirty(app);
            profiles_update_labels(app);
        }
        return true;
    }

    if(event.event >= EVENT_LONG_OFFSET && event.event < EVENT_LONG_OFFSET + count) {
        uint8_t index = (uint8_t)(event.event - EVENT_LONG_OFFSET);
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfiles, index);
        dhikr_app_set_edit_profile(app, index); // the highlighted one, not the active one
        scene_manager_next_scene(app->scene_manager, DhikrSceneProfileActions);
        return true;
    }

    if(event.event == ITEM_ADD) {
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfiles, ITEM_ADD);
        if(!dhikr_state_can_add(&app->state)) {
            dhikr_app_show_notice(
                app,
                "Profile limit",
                "All 16 profile slots\n"
                "are in use. Delete a\n"
                "custom profile first.");
        } else {
            app->name_mode = DhikrNameModeAdd;
            app->pending_name[0] = '\0';
            scene_manager_next_scene(app->scene_manager, DhikrSceneProfileName);
        }
        return true;
    }

    if(event.event == ITEM_EDIT) {
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfiles, ITEM_EDIT);
        scene_manager_next_scene(app->scene_manager, DhikrSceneProfileEdit);
        return true;
    }
    return false;
}

void dhikr_scene_profiles_on_exit(void* context) {
    DhikrApp* app = context;
    submenu_reset(app->submenu);
}
