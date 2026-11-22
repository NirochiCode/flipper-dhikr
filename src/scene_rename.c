#include "include/dhikr_app.h"

#include <stdio.h>

static void rename_result_callback(void* context) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DhikrEventTextEntered);
}

static bool rename_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);
    if(dhikr_profile_name_is_valid(text)) return true;
    furi_string_set(error, "Enter a name\n(letters/digits)");
    return false;
}

void dhikr_scene_rename_on_enter(void* context) {
    DhikrApp* app = context;
    const DhikrProfile* profile = &app->state.profiles[app->edit_profile];

    snprintf(app->text_buffer, sizeof(app->text_buffer), "%s", profile->name);

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Profile name (max 16)");
    text_input_set_minimum_length(app->text_input, 1);
    text_input_set_validator(app->text_input, rename_validator, app);
    text_input_set_result_callback(
        app->text_input,
        rename_result_callback,
        app,
        app->text_buffer,
        sizeof(app->text_buffer),
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewTextInput);
}

bool dhikr_scene_rename_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event != DhikrEventTextEntered) {
        return false;
    }

    DhikrProfile* profile = &app->state.profiles[app->edit_profile];
    if(dhikr_profile_rename(profile, app->text_buffer)) {
        dhikr_app_mark_dirty(app);
    }
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void dhikr_scene_rename_on_exit(void* context) {
    DhikrApp* app = context;
    text_input_reset(app->text_input);
}
