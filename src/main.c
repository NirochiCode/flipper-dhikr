/**
 * @file main.c
 * @brief Flipper Dhikr entry point: allocation, main loop, save policy, cleanup.
 */
#include "include/dhikr_app.h"
#include "include/storage_flipper.h"

#define TAG "FlipperDhikr"

/* ------------------------------------------------------------------------- */
/* Save policy                                                               */
/* ------------------------------------------------------------------------- */

void dhikr_app_mark_dirty(DhikrApp* app) {
    if(!app->dirty) {
        app->dirty = true;
        app->dirty_since = furi_get_tick();
    }
}

static void dhikr_app_save(DhikrApp* app) {
    DhikrSaveResult result = dhikr_store_save(&app->store, &app->state);
    if(result == DhikrSaveFailed) {
        // Keep the dirty flag; retry after the autosave interval, not on every tick.
        app->last_save_failed = true;
        app->dirty = true;
        app->dirty_since = furi_get_tick();
        FURI_LOG_W(TAG, "Saving failed");
    } else {
        app->last_save_failed = false;
        app->dirty = false;
        if(result == DhikrSaveWritten) FURI_LOG_D(TAG, "Saved");
    }
}

void dhikr_app_save_if_dirty(DhikrApp* app) {
    if(app->dirty) dhikr_app_save(app);
}

void dhikr_app_autosave_tick(DhikrApp* app) {
    if(!app->dirty || app->store.read_only) return;
    if(furi_get_tick() - app->dirty_since >= furi_ms_to_ticks(DHIKR_AUTOSAVE_INTERVAL_MS)) {
        dhikr_app_save(app);
    }
}

/* ------------------------------------------------------------------------- */
/* View dispatcher callbacks                                                 */
/* ------------------------------------------------------------------------- */

static bool dhikr_custom_event_callback(void* context, uint32_t event) {
    DhikrApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool dhikr_back_event_callback(void* context) {
    DhikrApp* app = context;
    // Returns false on the main menu, which stops the dispatcher and exits the app.
    return scene_manager_handle_back_event(app->scene_manager);
}

static void dhikr_tick_event_callback(void* context) {
    DhikrApp* app = context;
    dhikr_app_autosave_tick(app);
}

/* ------------------------------------------------------------------------- */
/* Lifecycle                                                                 */
/* ------------------------------------------------------------------------- */

static void dhikr_app_load(DhikrApp* app) {
    dhikr_flipper_file_io_init(&app->file_io, app->storage);
    dhikr_store_init(&app->store, &app->file_io);
    app->load_result = dhikr_store_load(&app->store, &app->state);
    FURI_LOG_I(TAG, "Load result: %d", app->load_result);

    if(app->load_result != DhikrLoadOk) {
        // First start, recovered or reset to defaults: persist a valid file right away.
        dhikr_app_mark_dirty(app);
        if(!app->store.read_only) dhikr_app_save(app);
    }
}

static DhikrApp* dhikr_app_alloc(void) {
    DhikrApp* app = malloc(sizeof(DhikrApp));
    memset(app, 0, sizeof(DhikrApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    dhikr_app_load(app);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&dhikr_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, dhikr_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, dhikr_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, dhikr_tick_event_callback, DHIKR_TICK_PERIOD_MS);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, DhikrViewSubmenu, submenu_get_view(app->submenu));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        DhikrViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DhikrViewNumberInput, number_input_get_view(app->number_input));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DhikrViewTextInput, text_input_get_view(app->text_input));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, DhikrViewWidget, widget_get_view(app->widget));

    app->counter_view = dhikr_counter_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DhikrViewCounter, dhikr_counter_view_get_view(app->counter_view));

    app->confirm_view = dhikr_confirm_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DhikrViewConfirm, dhikr_confirm_view_get_view(app->confirm_view));

    return app;
}

static void dhikr_app_free(DhikrApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, DhikrViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, DhikrViewVariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, DhikrViewNumberInput);
    view_dispatcher_remove_view(app->view_dispatcher, DhikrViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, DhikrViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, DhikrViewCounter);
    view_dispatcher_remove_view(app->view_dispatcher, DhikrViewConfirm);

    submenu_free(app->submenu);
    variable_item_list_free(app->variable_item_list);
    number_input_free(app->number_input);
    text_input_free(app->text_input);
    widget_free(app->widget);
    dhikr_counter_view_free(app->counter_view);
    dhikr_confirm_view_free(app->confirm_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t flipper_dhikr_app(void* p) {
    UNUSED(p);
    DhikrApp* app = dhikr_app_alloc();

    scene_manager_next_scene(app->scene_manager, DhikrSceneMainMenu);
    if(app->load_result != DhikrLoadOk && app->load_result != DhikrLoadMissing &&
       app->load_result != DhikrLoadRecovered) {
        scene_manager_next_scene(app->scene_manager, DhikrSceneNotice);
    }

    view_dispatcher_run(app->view_dispatcher);

    // Normal exit: make sure the latest progress is on the SD card.
    dhikr_app_save_if_dirty(app);
    dhikr_app_free(app);
    return 0;
}
