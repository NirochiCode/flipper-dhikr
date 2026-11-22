/**
 * @file dhikr_app.h
 * @brief Application context shared by all scenes.
 */
#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/number_input.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <notification/notification.h>
#include <storage/storage.h>

#include "include/profiles.h"
#include "include/scenes.h"
#include "include/storage.h"
#include "include/ui.h"

#define DHIKR_APP_NAME    "Flipper Dhikr"
#define DHIKR_APP_VERSION "1.0"

/** Unsaved counts are written at most this long after the first change. */
#define DHIKR_AUTOSAVE_INTERVAL_MS 15000
#define DHIKR_TICK_PERIOD_MS       1000

/** Room for the menu labels built at runtime. */
#define DHIKR_LABEL_SIZE 40

typedef enum {
    DhikrViewSubmenu,
    DhikrViewVariableItemList,
    DhikrViewNumberInput,
    DhikrViewTextInput,
    DhikrViewWidget,
    DhikrViewCounter,
    DhikrViewConfirm,
} DhikrViewId;

/** Menu item indexes are sent as-is; everything else starts above this base. */
typedef enum {
    DhikrEventBase = 1000,
    DhikrEventCounterIncrement,
    DhikrEventCounterPrev,
    DhikrEventCounterNext,
    DhikrEventConfirmAccepted,
    DhikrEventConfirmCancelled,
    DhikrEventNumberEntered,
    DhikrEventTextEntered,
    DhikrEventNoticeDismissed,
} DhikrEvent;

typedef struct {
    Gui* gui;
    Storage* storage;
    NotificationApp* notifications;

    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Submenu* submenu;
    VariableItemList* variable_item_list;
    NumberInput* number_input;
    TextInput* text_input;
    Widget* widget;
    DhikrCounterView* counter_view;
    DhikrConfirmView* confirm_view;

    DhikrState state;
    DhikrFileIo file_io;
    DhikrStore store;
    DhikrLoadResult load_result;

    bool dirty;
    uint32_t dirty_since;
    bool last_save_failed;

    /* Scratch data for the input scenes. */
    uint8_t edit_profile;
    int32_t number_value;
    char text_buffer[DHIKR_PROFILE_NAME_SIZE];
    char header[DHIKR_LABEL_SIZE];
    char labels[DHIKR_PROFILE_MAX * 2][DHIKR_LABEL_SIZE];
} DhikrApp;

/** Record that the state changed and needs saving. */
void dhikr_app_mark_dirty(DhikrApp* app);

/** Save now if there are unsaved changes. */
void dhikr_app_save_if_dirty(DhikrApp* app);

/** Save if changes have been pending for DHIKR_AUTOSAVE_INTERVAL_MS. */
void dhikr_app_autosave_tick(DhikrApp* app);
