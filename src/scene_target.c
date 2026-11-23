/*
 * Target presets for the profile being edited (app->edit_profile), opened from
 * the profile edit menu ("Edit Target"):
 *   33 times / 99 times / Custom value...   ("*" marks the current target)
 * The count, the completed sets and the active profile are not changed.
 * Choosing a value saves it and returns to the profile edit menu.
 */
#include "include/dhikr_app.h"

#include <stdio.h>
#include <string.h>

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
    if(app->edit_profile >= app->state.profile_count) app->edit_profile = 0;
    const DhikrProfile* profile = &app->state.profiles[app->edit_profile];
    const uint32_t target = profile->counter.target;
    const bool is_a = target == DHIKR_TARGET_PRESET_A;
    const bool is_b = target == DHIKR_TARGET_PRESET_B;

    // Local copy: source and destination both live inside `app`.
    char name[DHIKR_PROFILE_NAME_SIZE];
    memcpy(name, profile->name, sizeof(name));

    submenu_reset(submenu);
    snprintf(app->header, sizeof(app->header), "Target: %s", name);
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

    switch(event.event) {
    case TargetItemPresetA:
    case TargetItemPresetB: {
        uint32_t target = event.event == TargetItemPresetA ? DHIKR_TARGET_PRESET_A :
                                                             DHIKR_TARGET_PRESET_B;
        if(dhikr_state_set_target(&app->state, app->edit_profile, target)) {
            dhikr_app_mark_dirty(app);
            dhikr_app_save_if_dirty(app);
        }
        scene_manager_previous_scene(app->scene_manager); // profile edit menu
        return true;
    }
    case TargetItemCustom:
        app->target_mode = DhikrTargetModeEditProfile;
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
