/*
 * Counter Feedback / Target Feedback: two independent ON/OFF switches.
 *   OK on "Vibration: ON/OFF" or "Sound: ON/OFF" toggles it (label updates at once)
 *   OK on "Preview" plays this event's pattern with the current switches
 *   Back returns to Feedback Settings; changes are saved on leaving
 */
#include "include/dhikr_app.h"
#include "include/feedback_player.h"

#include <stdio.h>

typedef enum {
    SwitchItemVibration,
    SwitchItemSound,
    SwitchItemPreview,
} SwitchItem;

static bool* feedback_switch(DhikrApp* app, SwitchItem item) {
    DhikrFeedbackSettings* settings = &app->state.feedback;
    const bool target = app->feedback_edit_event == DhikrFeedbackEventTarget;
    if(item == SwitchItemVibration) {
        return target ? &settings->target_vibration : &settings->counter_vibration;
    }
    return target ? &settings->target_sound : &settings->counter_sound;
}

static void feedback_event_update_label(DhikrApp* app, SwitchItem item) {
    snprintf(
        app->labels[item],
        DHIKR_LABEL_SIZE,
        "%s: %s",
        item == SwitchItemVibration ? "Vibration" : "Sound",
        *feedback_switch(app, item) ? "ON" : "OFF");
}

static void feedback_event_callback(void* context, uint32_t index) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dhikr_scene_feedback_event_on_enter(void* context) {
    DhikrApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(
        submenu,
        app->feedback_edit_event == DhikrFeedbackEventTarget ? "Target Feedback" :
                                                               "Counter Feedback");
    feedback_event_update_label(app, SwitchItemVibration);
    feedback_event_update_label(app, SwitchItemSound);
    submenu_add_item(
        submenu, app->labels[SwitchItemVibration], SwitchItemVibration, feedback_event_callback, app);
    submenu_add_item(
        submenu, app->labels[SwitchItemSound], SwitchItemSound, feedback_event_callback, app);
    submenu_add_item(submenu, "Preview", SwitchItemPreview, feedback_event_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, DhikrSceneFeedbackEvent));
    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewSubmenu);
}

bool dhikr_scene_feedback_event_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    switch(event.event) {
    case SwitchItemVibration:
    case SwitchItemSound: {
        SwitchItem item = (SwitchItem)event.event;
        bool* value = feedback_switch(app, item);
        *value = !*value;
        feedback_event_update_label(app, item);
        submenu_change_item_label(app->submenu, item, app->labels[item]);
        dhikr_app_mark_dirty(app);
        return true;
    }
    case SwitchItemPreview:
        dhikr_feedback_play(app->notifications, &app->state.feedback, app->feedback_edit_event);
        return true;
    default:
        return false;
    }
}

void dhikr_scene_feedback_event_on_exit(void* context) {
    DhikrApp* app = context;
    submenu_reset(app->submenu);
    dhikr_app_save_if_dirty(app);
}
