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

#include "include/counter_input.h"
#include "include/profiles.h"
#include "include/scenes.h"
#include "include/storage.h"
#include "include/ui.h"

#define DHIKR_APP_NAME    "Flipper Dhikr"
#define DHIKR_APP_VERSION "1.0.2"

/** Unsaved counts are written at most this long after the first change. */
#define DHIKR_AUTOSAVE_INTERVAL_MS 15000
#define DHIKR_TICK_PERIOD_MS       1000
/** Immediate saves (completed round, undo) at most this often; otherwise autosave. */
#define DHIKR_IMMEDIATE_SAVE_MIN_INTERVAL_MS 3000
/** Feedback is skipped while the target pattern still plays (keeps the queue short). */
#define DHIKR_TARGET_FEEDBACK_MS 650

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
    DhikrEventConfirmAccepted,
    DhikrEventConfirmCancelled,
    DhikrEventNumberEntered,
    DhikrEventTextEntered,
    DhikrEventNoticeDismissed,
    /** + DhikrCounterAction */
    DhikrEventCounterActionBase = 1100,
} DhikrEvent;

/** What the shared number input is used for. */
typedef enum {
    /** Custom target of the active profile (Target Settings). */
    DhikrTargetModeActive,
    /** Custom target of `edit_profile` (profile edit menu). */
    DhikrTargetModeEditProfile,
    /** Target of a profile that is being created (name in `pending_name`). */
    DhikrTargetModeNewProfile,
} DhikrTargetMode;

/** What the shared name input is used for. */
typedef enum {
    DhikrNameModeAdd,
    DhikrNameModeRename,
} DhikrNameMode;

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
    bool has_saved;
    uint32_t last_save_tick;

    /* Counter screen banner ("Target reached!", "Undo: -1", ...). */
    DhikrBanner banner;
    /** dhikr_ui_now_ms() until which the target pattern is still playing. */
    uint32_t target_feedback_until_ms;

    /* Scratch data for the input and edit scenes. */
    uint8_t edit_profile;
    DhikrTargetMode target_mode;
    DhikrNameMode name_mode;
    int32_t number_value;
    char text_buffer[DHIKR_PROFILE_NAME_SIZE];
    char pending_name[DHIKR_PROFILE_NAME_SIZE];
    char header[DHIKR_LABEL_SIZE];
    char labels[DHIKR_PROFILE_MAX + 4][DHIKR_LABEL_SIZE];

    /** Which switches the FeedbackEvent screen edits (count or target). */
    DhikrFeedbackEvent feedback_edit_event;

    /* Text for the generic notice screen (string literals). */
    const char* notice_title;
    const char* notice_text;
} DhikrApp;

/** Record that the state changed and needs saving. */
void dhikr_app_mark_dirty(DhikrApp* app);

/** Save now if there are unsaved changes. */
void dhikr_app_save_if_dirty(DhikrApp* app);

/** Show the generic notice screen; OK or Back returns to the previous screen. */
void dhikr_app_show_notice(DhikrApp* app, const char* title, const char* text);

/**
 * Save now unless a save happened within DHIKR_IMMEDIATE_SAVE_MIN_INTERVAL_MS;
 * in that case the change is left to the autosave (rate limit for tiny targets).
 */
void dhikr_app_save_soon(DhikrApp* app);

/** Save if changes have been pending for DHIKR_AUTOSAVE_INTERVAL_MS. */
void dhikr_app_autosave_tick(DhikrApp* app);
