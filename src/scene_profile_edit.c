/* "Edit Profiles...": choose a profile to rename, retarget or delete. */
#include "include/dhikr_app.h"

#include <stdio.h>
#include <string.h>

static void profile_edit_callback(void* context, uint32_t index) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dhikr_scene_profile_edit_on_enter(void* context) {
    DhikrApp* app = context;
    Submenu* submenu = app->submenu;
    const DhikrState* state = &app->state;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Edit Profiles");
    for(uint8_t i = 0; i < state->profile_count; i++) {
        const DhikrProfile* profile = &state->profiles[i];
        // Local copy: source and destination both live inside `app`.
        char name[DHIKR_PROFILE_NAME_SIZE];
        memcpy(name, profile->name, sizeof(name));
        snprintf(
            app->labels[i], DHIKR_LABEL_SIZE, "%s%s", name, profile->builtin ? " (fixed)" : "");
        submenu_add_item(submenu, app->labels[i], i, profile_edit_callback, app);
    }

    uint32_t selected = scene_manager_get_scene_state(app->scene_manager, DhikrSceneProfileEdit);
    if(selected >= state->profile_count) selected = 0;
    submenu_set_selected_item(submenu, selected);
    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewSubmenu);
}

bool dhikr_scene_profile_edit_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event >= app->state.profile_count) return false;

    scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfileEdit, event.event);
    app->edit_profile = (uint8_t)event.event;
    scene_manager_next_scene(app->scene_manager, DhikrSceneProfileActions);
    return true;
}

void dhikr_scene_profile_edit_on_exit(void* context) {
    DhikrApp* app = context;
    submenu_reset(app->submenu);
}
