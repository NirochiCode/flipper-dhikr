/*
 * Feedback Settings (overview):
 *   Counter Feedback >   -> FeedbackEvent (count switches)
 *   Target Feedback  >   -> FeedbackEvent (target switches)
 *   Stealth: Off / ON (muted)   (read-only status, OK opens the help)
 *   Help
 */
#include "include/dhikr_app.h"
#include "include/feedback_player.h"

typedef enum {
    FeedbackItemCounter,
    FeedbackItemTarget,
    FeedbackItemStealth,
    FeedbackItemHelp,
} FeedbackItem;

static const char* const feedback_help_text = "Uses system settings:\n"
                                              "Notifications > Vibro,\n"
                                              "Volume; Stealth mode\n"
                                              "mutes everything.";

static void feedback_menu_callback(void* context, uint32_t index) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dhikr_scene_feedback_on_enter(void* context) {
    DhikrApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Feedback Settings");
    submenu_add_item(submenu, "Counter Feedback  >", FeedbackItemCounter, feedback_menu_callback, app);
    submenu_add_item(submenu, "Target Feedback  >", FeedbackItemTarget, feedback_menu_callback, app);
    submenu_add_item(
        submenu,
        dhikr_feedback_system_muted() ? "Stealth: ON (muted)" : "Stealth: Off",
        FeedbackItemStealth,
        feedback_menu_callback,
        app);
    submenu_add_item(submenu, "Help", FeedbackItemHelp, feedback_menu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, DhikrSceneFeedback));
    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewSubmenu);
}

bool dhikr_scene_feedback_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        // Leaving to the menu: open at the top next time.
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneFeedback, 0);
        return false;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, DhikrSceneFeedback, event.event);
    switch(event.event) {
    case FeedbackItemCounter:
    case FeedbackItemTarget:
        app->feedback_edit_event = event.event == FeedbackItemCounter ? DhikrFeedbackEventCount :
                                                                        DhikrFeedbackEventTarget;
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneFeedbackEvent, 0);
        scene_manager_next_scene(app->scene_manager, DhikrSceneFeedbackEvent);
        return true;
    case FeedbackItemStealth:
    case FeedbackItemHelp:
        dhikr_app_show_notice(app, "Feedback help", feedback_help_text);
        return true;
    default:
        return false;
    }
}

void dhikr_scene_feedback_on_exit(void* context) {
    DhikrApp* app = context;
    submenu_reset(app->submenu);
}
