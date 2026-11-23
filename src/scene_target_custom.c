/*
 * Number input for a target (1-9999). Used for:
 *  - any profile from its edit menu (Edit Target -> Custom value...)
 *  - a new profile (second step of "+ Add Profile")
 */
#include "include/dhikr_app.h"

static void target_number_callback(void* context, int32_t number) {
    DhikrApp* app = context;
    app->number_value = number;
    view_dispatcher_send_custom_event(app->view_dispatcher, DhikrEventNumberEntered);
}

static const DhikrCounter* target_counter(DhikrApp* app) {
    if(app->edit_profile >= app->state.profile_count) app->edit_profile = 0;
    return &app->state.profiles[app->edit_profile].counter;
}

void dhikr_scene_target_custom_on_enter(void* context) {
    DhikrApp* app = context;
    const bool is_new = app->target_mode == DhikrTargetModeNewProfile;
    int32_t current = is_new ? (int32_t)DHIKR_TARGET_DEFAULT : (int32_t)target_counter(app)->target;

    number_input_set_header_text(
        app->number_input, is_new ? "Target for new profile" : "Target (1-9999)");
    number_input_set_result_callback(
        app->number_input,
        target_number_callback,
        app,
        current,
        (int32_t)DHIKR_TARGET_MIN,
        (int32_t)DHIKR_TARGET_MAX);

    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewNumberInput);
}

static void target_custom_create_profile(DhikrApp* app, uint32_t target) {
    uint8_t index = 0;
    DhikrProfileError error = dhikr_state_add_custom(&app->state, app->pending_name, target, &index);

    if(error == DhikrProfileOk) {
        // The new profile becomes active and is highlighted in the list.
        dhikr_state_select(&app->state, index);
        dhikr_app_mark_dirty(app);
        dhikr_app_save_if_dirty(app);
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfiles, index);
    }

    // Back to the profile list (rebuilt on enter, so it includes the new profile).
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, DhikrSceneProfiles);

    if(error == DhikrProfileErrorFull) {
        dhikr_app_show_notice(app, "Profile limit", "All 16 profile slots\nare in use.");
    } else if(error != DhikrProfileOk) {
        dhikr_app_show_notice(app, "Not created", "The name is empty or\nalready in use.");
    }
}

bool dhikr_scene_target_custom_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event != DhikrEventNumberEntered) {
        return false;
    }

    // NumberInput enforces the range, but never trust it blindly.
    if(app->number_value < (int32_t)DHIKR_TARGET_MIN ||
       app->number_value > (int32_t)DHIKR_TARGET_MAX) {
        return true; // stay on the input screen; Back cancels
    }
    const uint32_t target = (uint32_t)app->number_value;

    if(app->target_mode == DhikrTargetModeNewProfile) {
        target_custom_create_profile(app, target);
        return true;
    }

    // Edit Target of `edit_profile`: count, sets and the active profile stay unchanged.
    if(dhikr_state_set_target(&app->state, app->edit_profile, target)) {
        dhikr_app_mark_dirty(app);
        dhikr_app_save_if_dirty(app);
    }
    // Back to that profile's edit menu (skipping the preset list).
    scene_manager_search_and_switch_to_previous_scene(
        app->scene_manager, DhikrSceneProfileActions);
    return true;
}

void dhikr_scene_target_custom_on_exit(void* context) {
    UNUSED(context);
}
