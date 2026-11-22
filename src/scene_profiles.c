#include "include/dhikr_app.h"

#include <stdio.h>
#include <string.h>

/* Item indexes: 0..n-1 select a profile, RENAME_BASE + i renames profile i. */
#define RENAME_BASE DHIKR_PROFILE_MAX

static void profiles_menu_callback(void* context, uint32_t index) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dhikr_scene_profiles_on_enter(void* context) {
    DhikrApp* app = context;
    Submenu* submenu = app->submenu;
    DhikrState* state = &app->state;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Dhikr Profiles");

    for(uint8_t i = 0; i < state->profile_count; i++) {
        const DhikrProfile* profile = &state->profiles[i];
        // Local copy: source and destination both live inside `app`.
        char name[DHIKR_PROFILE_NAME_SIZE];
        memcpy(name, profile->name, sizeof(name));
        snprintf(
            app->labels[i],
            DHIKR_LABEL_SIZE,
            "%s%s: %lu",
            i == state->active ? "* " : "",
            name,
            (unsigned long)profile->counter.count);
        submenu_add_item(submenu, app->labels[i], i, profiles_menu_callback, app);
    }

    for(uint8_t i = 0; i < state->profile_count; i++) {
        const DhikrProfile* profile = &state->profiles[i];
        if(!profile->name_editable) continue;
        char name[DHIKR_PROFILE_NAME_SIZE];
        memcpy(name, profile->name, sizeof(name));
        snprintf(app->labels[RENAME_BASE + i], DHIKR_LABEL_SIZE, "Rename \"%s\"", name);
        submenu_add_item(
            submenu, app->labels[RENAME_BASE + i], RENAME_BASE + i, profiles_menu_callback, app);
    }

    submenu_set_selected_item(submenu, state->active);
    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewSubmenu);
}

bool dhikr_scene_profiles_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event < app->state.profile_count) {
        if(dhikr_state_select(&app->state, (uint8_t)event.event)) {
            dhikr_app_mark_dirty(app);
        }
        // Back to the main menu with "Start Counting" highlighted.
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneMainMenu, 0);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    if(event.event >= RENAME_BASE && event.event < (uint32_t)RENAME_BASE + app->state.profile_count) {
        app->edit_profile = (uint8_t)(event.event - RENAME_BASE);
        scene_manager_next_scene(app->scene_manager, DhikrSceneRename);
        return true;
    }
    return false;
}

void dhikr_scene_profiles_on_exit(void* context) {
    DhikrApp* app = context;
    submenu_reset(app->submenu);
}
