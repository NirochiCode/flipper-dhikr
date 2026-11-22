#include "include/dhikr_app.h"

typedef enum {
    MainMenuStartCounting,
    MainMenuProfiles,
    MainMenuTarget,
    MainMenuFeedback,
    MainMenuReset,
    MainMenuAbout,
} MainMenuItem;

static void main_menu_callback(void* context, uint32_t index) {
    DhikrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void dhikr_scene_main_menu_on_enter(void* context) {
    DhikrApp* app = context;
    Submenu* submenu = app->submenu;

    // Returning to the menu is a natural point to persist changes.
    dhikr_app_save_if_dirty(app);

    submenu_reset(submenu);
    submenu_set_header(submenu, DHIKR_APP_NAME);
    submenu_add_item(submenu, "Start Counting", MainMenuStartCounting, main_menu_callback, app);
    submenu_add_item(submenu, "Dhikr Profiles", MainMenuProfiles, main_menu_callback, app);
    submenu_add_item(submenu, "Target Settings", MainMenuTarget, main_menu_callback, app);
    submenu_add_item(submenu, "Feedback Settings", MainMenuFeedback, main_menu_callback, app);
    submenu_add_item(submenu, "Reset Counter", MainMenuReset, main_menu_callback, app);
    submenu_add_item(submenu, "About", MainMenuAbout, main_menu_callback, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, DhikrSceneMainMenu));

    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewSubmenu);
}

bool dhikr_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    static const DhikrScene targets[] = {
        [MainMenuStartCounting] = DhikrSceneCounter,
        [MainMenuProfiles] = DhikrSceneProfiles,
        [MainMenuTarget] = DhikrSceneTarget,
        [MainMenuFeedback] = DhikrSceneFeedback,
        [MainMenuReset] = DhikrSceneReset,
        [MainMenuAbout] = DhikrSceneAbout,
    };
    if(event.event >= COUNT_OF(targets)) return false;

    scene_manager_set_scene_state(app->scene_manager, DhikrSceneMainMenu, event.event);
    scene_manager_next_scene(app->scene_manager, targets[event.event]);
    return true;
}

void dhikr_scene_main_menu_on_exit(void* context) {
    DhikrApp* app = context;
    submenu_reset(app->submenu);
}
