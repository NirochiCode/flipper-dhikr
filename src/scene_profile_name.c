/* Name input for "+ Add Profile" (then target) and "Rename". */
#include "include/dhikr_app.h"

#include <stdio.h>

static void profile_name_result_callback(void* context) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DhikrEventTextEntered);
}

static bool profile_name_validator(const char* text, FuriString* error, void* context) {
    DhikrApp* app = context;
    int exclude = app->name_mode == DhikrNameModeRename ? app->edit_profile : -1;
    switch(dhikr_state_check_name(&app->state, text, exclude)) {
    case DhikrProfileOk:
        return true;
    case DhikrProfileErrorDuplicateName:
        furi_string_set(error, "Name already\nexists");
        return false;
    default:
        furi_string_set(error, "Enter 1-16\ncharacters");
        return false;
    }
}

void dhikr_scene_profile_name_on_enter(void* context) {
    DhikrApp* app = context;
    const bool rename = app->name_mode == DhikrNameModeRename;

    // A suggested name is replaced by the first key press; a real name is kept for editing.
    bool clear_on_type = false;
    if(rename) {
        snprintf(
            app->text_buffer,
            sizeof(app->text_buffer),
            "%s",
            app->state.profiles[app->edit_profile].name);
    } else if(app->pending_name[0] != '\0') {
        // Back from the target step: keep what was typed.
        snprintf(app->text_buffer, sizeof(app->text_buffer), "%s", app->pending_name);
    } else {
        dhikr_state_suggest_name(&app->state, app->text_buffer, sizeof(app->text_buffer));
        clear_on_type = true;
    }

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, rename ? "Rename (max 16)" : "New profile name");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_validator(app->text_input, profile_name_validator, app);
    text_input_set_result_callback(
        app->text_input,
        profile_name_result_callback,
        app,
        app->text_buffer,
        sizeof(app->text_buffer),
        clear_on_type);

    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewTextInput);
}

bool dhikr_scene_profile_name_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event != DhikrEventTextEntered) {
        return false;
    }

    if(app->name_mode == DhikrNameModeRename) {
        if(dhikr_state_rename(&app->state, app->edit_profile, app->text_buffer) ==
           DhikrProfileOk) {
            dhikr_app_mark_dirty(app);
            dhikr_app_save_if_dirty(app);
        }
        scene_manager_previous_scene(app->scene_manager);
    } else {
        // Second step of "+ Add Profile": choose the target.
        snprintf(app->pending_name, sizeof(app->pending_name), "%s", app->text_buffer);
        app->target_mode = DhikrTargetModeNewProfile;
        scene_manager_next_scene(app->scene_manager, DhikrSceneTargetCustom);
    }
    return true;
}

void dhikr_scene_profile_name_on_exit(void* context) {
    DhikrApp* app = context;
    text_input_reset(app->text_input);
}
