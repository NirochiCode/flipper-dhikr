#include "include/dhikr_app.h"

static const char* about_storage_status(const DhikrApp* app) {
    if(app->store.read_only) return "read-only (SD error)";
    if(app->last_save_failed) return "last save FAILED";
    return "OK";
}

void dhikr_scene_about_on_enter(void* context) {
    DhikrApp* app = context;
    FuriString* text = furi_string_alloc();

    furi_string_printf(
        text,
        "\e#%s v%s\n"
        "Digital tasbih counter\n"
        "with multiple profiles.\n"
        "\n"
        "\e#Counter keys\n"
        "OK: count +1\n"
        "Left/Right: switch profile\n"
        "Back: menu / exit\n"
        "\n"
        "\e#Saving\n"
        "Saved on leaving the\n"
        "counter, on exit and\n"
        "every %u s while counting.\n"
        "Folder: apps_data/\n"
        "flipper_dhikr\n"
        "Storage: %s\n"
        "\n"
        "Open source, MIT license.",
        DHIKR_APP_NAME,
        DHIKR_APP_VERSION,
        (unsigned)(DHIKR_AUTOSAVE_INTERVAL_MS / 1000),
        about_storage_status(app));

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewWidget);
}

bool dhikr_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void dhikr_scene_about_on_exit(void* context) {
    DhikrApp* app = context;
    widget_reset(app->widget);
}
