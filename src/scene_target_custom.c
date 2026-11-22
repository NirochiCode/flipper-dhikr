#include "include/dhikr_app.h"

static void target_number_callback(void* context, int32_t number) {
    DhikrApp* app = context;
    app->number_value = number;
    view_dispatcher_send_custom_event(app->view_dispatcher, DhikrEventNumberEntered);
}

void dhikr_scene_target_custom_on_enter(void* context) {
    DhikrApp* app = context;
    const DhikrCounter* counter = &dhikr_state_active(&app->state)->counter;

    number_input_set_header_text(app->number_input, "Target (1-9999)");
    number_input_set_result_callback(
        app->number_input,
        target_number_callback,
        app,
        (int32_t)counter->target,
        (int32_t)DHIKR_TARGET_MIN,
        (int32_t)DHIKR_TARGET_MAX);

    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewNumberInput);
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

    DhikrCounter* counter = &dhikr_state_active(&app->state)->counter;
    uint32_t target = (uint32_t)app->number_value;
    if(counter->target != target && dhikr_counter_set_target(counter, target)) {
        dhikr_app_mark_dirty(app);
    }
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, DhikrSceneMainMenu);
    return true;
}

void dhikr_scene_target_custom_on_exit(void* context) {
    UNUSED(context);
}
