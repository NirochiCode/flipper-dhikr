#include "include/dhikr_app.h"

#include <stdio.h>

typedef enum {
    TargetItemPresetA,
    TargetItemPresetB,
    TargetItemCustom,
} TargetItem;

static void target_menu_callback(void* context, uint32_t index) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dhikr_scene_target_on_enter(void* context) {
    DhikrApp* app = context;
    Submenu* submenu = app->submenu;
    const DhikrProfile* profile = dhikr_state_active(&app->state);
    const uint32_t target = profile->counter.target;
    const bool is_a = target == DHIKR_TARGET_PRESET_A;
    const bool is_b = target == DHIKR_TARGET_PRESET_B;

    submenu_reset(submenu);
    snprintf(app->header, sizeof(app->header), "Target: %s", profile->name);
    submenu_set_header(submenu, app->header);

    snprintf(
        app->labels[0],
        DHIKR_LABEL_SIZE,
        "%s%lu times",
        is_a ? "* " : "",
        (unsigned long)DHIKR_TARGET_PRESET_A);
    snprintf(
        app->labels[1],
        DHIKR_LABEL_SIZE,
        "%s%lu times",
        is_b ? "* " : "",
        (unsigned long)DHIKR_TARGET_PRESET_B);
    if(is_a || is_b) {
        snprintf(app->labels[2], DHIKR_LABEL_SIZE, "Custom value...");
    } else {
        snprintf(app->labels[2], DHIKR_LABEL_SIZE, "* Custom: %lu", (unsigned long)target);
    }

    submenu_add_item(submenu, app->labels[0], TargetItemPresetA, target_menu_callback, app);
    submenu_add_item(submenu, app->labels[1], TargetItemPresetB, target_menu_callback, app);
    submenu_add_item(submenu, app->labels[2], TargetItemCustom, target_menu_callback, app);
    submenu_set_selected_item(
        submenu, is_a ? TargetItemPresetA : (is_b ? TargetItemPresetB : TargetItemCustom));

    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewSubmenu);
}

bool dhikr_scene_target_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    DhikrCounter* counter = &dhikr_state_active(&app->state)->counter;
    switch(event.event) {
    case TargetItemPresetA:
    case TargetItemPresetB: {
        // Changing the target never touches the current count.
        uint32_t target = event.event == TargetItemPresetA ? DHIKR_TARGET_PRESET_A :
                                                             DHIKR_TARGET_PRESET_B;
        if(counter->target != target && dhikr_counter_set_target(counter, target)) {
            dhikr_app_mark_dirty(app);
        }
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    case TargetItemCustom:
        app->target_mode = DhikrTargetModeActive;
        scene_manager_next_scene(app->scene_manager, DhikrSceneTargetCustom);
        return true;
    default:
        return false;
    }
}

void dhikr_scene_target_on_exit(void* context) {
    DhikrApp* app = context;
    submenu_reset(app->submenu);
}
