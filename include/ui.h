/**
 * @file ui.h
 * @brief Custom views: the counter screen and the [No] [Yes] confirmation.
 *
 * Views only draw and translate key presses into events; all state changes
 * happen in the scenes. Only InputTypeShort is acted upon, so a key press is
 * handled exactly once (no auto-repeat, no long-press side effects).
 */
#pragma once

#include <gui/view.h>

#include "include/counter_input.h"
#include "include/profiles.h"

/* --- Counter view ------------------------------------------------------------ */

/** Receives the mapped action (see counter_input.h); never DhikrCounterActionNone/Exit. */
typedef void (*DhikrCounterViewCallback)(DhikrCounterAction action, void* context);

typedef struct DhikrCounterView DhikrCounterView;

DhikrCounterView* dhikr_counter_view_alloc(void);
void dhikr_counter_view_free(DhikrCounterView* counter_view);
View* dhikr_counter_view_get_view(DhikrCounterView* counter_view);
void dhikr_counter_view_set_callback(
    DhikrCounterView* counter_view,
    DhikrCounterViewCallback callback,
    void* context);
/**
 * Copy what is needed to draw the given profile. A visible banner is drawn
 * until it expires; a one-shot timer redraws the screen at that moment.
 * @param muted show a "Muted" hint (feedback enabled but stealth mode on)
 */
void dhikr_counter_view_update(
    DhikrCounterView* counter_view,
    const DhikrProfile* profile,
    uint8_t profile_count,
    const DhikrBanner* banner,
    bool muted);

/** Milliseconds since boot, for banner timing. */
uint32_t dhikr_ui_now_ms(void);

/* --- Confirm view ------------------------------------------------------------ */

typedef enum {
    DhikrConfirmViewEventAccepted,
    DhikrConfirmViewEventCancelled,
} DhikrConfirmViewEvent;

typedef void (*DhikrConfirmViewCallback)(DhikrConfirmViewEvent event, void* context);

typedef struct DhikrConfirmView DhikrConfirmView;

DhikrConfirmView* dhikr_confirm_view_alloc(void);
void dhikr_confirm_view_free(DhikrConfirmView* confirm_view);
View* dhikr_confirm_view_get_view(DhikrConfirmView* confirm_view);
void dhikr_confirm_view_set_callback(
    DhikrConfirmView* confirm_view,
    DhikrConfirmViewCallback callback,
    void* context);
/** Set texts and reset the selection to "No". */
void dhikr_confirm_view_setup(
    DhikrConfirmView* confirm_view,
    const char* title,
    const char* line1,
    const char* line2);
