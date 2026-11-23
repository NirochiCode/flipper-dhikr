/*
 * Main menu. Targets are edited per profile (Dhikr Profiles -> hold OK or
 * Edit Profiles... -> Edit Target), so there is no separate target entry.
 */
#include "include/dhikr_app.h"
#include "include/menu.h"

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
    for(uint32_t item = 0; item < DhikrMainMenuCount; item++) {
        submenu_add_item(
            submenu,
            dhikr_main_menu_label((DhikrMainMenuItem)item),
            item,
            main_menu_callback,
            app);
    }
    uint32_t selected = scene_manager_get_scene_state(app->scene_manager, DhikrSceneMainMenu);
    if(selected >= DhikrMainMenuCount) selected = 0;
    submenu_set_selected_item(submenu, selected);

    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewSubmenu);
}

bool dhikr_scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    static const DhikrScene targets[DhikrMainMenuCount] = {
        [DhikrMainMenuStartCounting] = DhikrSceneCounter,
        [DhikrMainMenuProfiles] = DhikrSceneProfiles,
        [DhikrMainMenuFeedback] = DhikrSceneFeedback,
        [DhikrMainMenuReset] = DhikrSceneReset,
        [DhikrMainMenuAbout] = DhikrSceneAbout,
    };
    if(event.event >= COUNT_OF(targets)) return false;

    scene_manager_set_scene_state(app->scene_manager, DhikrSceneMainMenu, event.event);
    if(event.event == DhikrMainMenuProfiles) {
        // Open the profile list on the active profile.
        scene_manager_set_scene_state(app->scene_manager, DhikrSceneProfiles, 0);
    }
    scene_manager_next_scene(app->scene_manager, targets[event.event]);
    return true;
}

void dhikr_scene_main_menu_on_exit(void* context) {
    DhikrApp* app = context;
    submenu_reset(app->submenu);
}
