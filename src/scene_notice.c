/* Shown once at start-up when saved data could not be used. */
#include "include/dhikr_app.h"

static void notice_button_callback(GuiButtonType result, InputType type, void* context) {
    DhikrApp* app = context;
    if(result == GuiButtonTypeCenter && type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DhikrEventNoticeDismissed);
    }
}

static const char* notice_text(const DhikrApp* app) {
    if(app->store.read_only) {
        return app->load_result == DhikrLoadIoError ? "SD card not readable.\n"
                                                      "Defaults loaded. Progress\n"
                                                      "will NOT be saved now." :
                                                      "Saved data is invalid and\n"
                                                      "could not be moved away.\n"
                                                      "Saving disabled.";
    }
    if(app->load_result == DhikrLoadUnsupported) {
        return "Data from a newer app\n"
               "version. Defaults loaded.\n"
               "Old file: settings.bak";
    }
    return "Saved data was damaged.\n"
           "Defaults loaded.\n"
           "Old file: settings.bak";
}

void dhikr_scene_notice_on_enter(void* context) {
    DhikrApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Storage notice");
    widget_add_string_multiline_element(
        app->widget, 64, 16, AlignCenter, AlignTop, FontSecondary, notice_text(app));
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "OK", notice_button_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewWidget);
}

bool dhikr_scene_notice_on_event(void* context, SceneManagerEvent event) {
    DhikrApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == DhikrEventNoticeDismissed) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void dhikr_scene_notice_on_exit(void* context) {
    DhikrApp* app = context;
    widget_reset(app->widget);
}
