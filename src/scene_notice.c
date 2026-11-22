/* Generic message screen (title + text from app->notice_*); OK or Back closes it. */
#include "include/dhikr_app.h"

static void notice_button_callback(GuiButtonType result, InputType type, void* context) {
    DhikrApp* app = context;
    if(result == GuiButtonTypeCenter && type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DhikrEventNoticeDismissed);
    }
}

void dhikr_scene_notice_on_enter(void* context) {
    DhikrApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget,
        64,
        2,
        AlignCenter,
        AlignTop,
        FontPrimary,
        app->notice_title ? app->notice_title : "");
    widget_add_string_multiline_element(
        app->widget,
        64,
        16,
        AlignCenter,
        AlignTop,
        FontSecondary,
        app->notice_text ? app->notice_text : "");
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
