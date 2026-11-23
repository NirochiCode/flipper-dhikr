/**
 * @file counter_input.h
 * @brief Hardware-independent key mapping and on-screen banner timing for the
 *        counter screen.
 *
 * Mapping:
 *   OK    short  -> +1
 *   Down  long   -> undo   (fires once per hold: Repeat events are ignored)
 *   Left  short  -> previous profile
 *   Right short  -> next profile
 *   Back  short  -> leave the counter (handled by the view dispatcher)
 * Everything else (press/release, repeat, long OK, Up) does nothing.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "include/feedback.h"
#include "include/profiles.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mirrors of the SDK's InputKey / InputType so this module stays SDK-free. */
typedef enum {
    DhikrKeyUp,
    DhikrKeyDown,
    DhikrKeyRight,
    DhikrKeyLeft,
    DhikrKeyOk,
    DhikrKeyBack,
    DhikrKeyOther,
} DhikrKey;

typedef enum {
    DhikrPressPress,
    DhikrPressRelease,
    DhikrPressShort,
    DhikrPressLong,
    DhikrPressRepeat,
    DhikrPressOther,
} DhikrPress;

typedef enum {
    DhikrCounterActionNone,
    DhikrCounterActionIncrement,
    DhikrCounterActionUndo,
    DhikrCounterActionPrevProfile,
    DhikrCounterActionNextProfile,
    DhikrCounterActionExit,
} DhikrCounterAction;

DhikrCounterAction dhikr_counter_input_map(DhikrKey key, DhikrPress press);

/* --- Banner ------------------------------------------------------------------ */

typedef enum {
    DhikrBannerNone,
    /** "Set N complete!" after a completed round (shown to the user as a "set"). */
    DhikrBannerTarget,
    /** "Undo -1" */
    DhikrBannerUndo,
    /** "Set undone" (undo of the completing count) */
    DhikrBannerRoundRestored,
    /** "Nothing to undo" */
    DhikrBannerNothingToUndo,
} DhikrBannerKind;

#define DHIKR_BANNER_TARGET_MS 1000UL
#define DHIKR_BANNER_INFO_MS   800UL

typedef struct {
    DhikrBannerKind kind;
    uint32_t started_ms;
    /** Value shown with the banner (the completed target for DhikrBannerTarget). */
    uint32_t value;
    /** DhikrBannerTarget: number of the set just completed (1 = first set). */
    uint32_t set_number;
} DhikrBanner;

void dhikr_banner_show(DhikrBanner* banner, DhikrBannerKind kind, uint32_t value, uint32_t now_ms);
void dhikr_banner_clear(DhikrBanner* banner);
/** Wrap-around safe. */
bool dhikr_banner_is_visible(const DhikrBanner* banner, uint32_t now_ms);
/** Milliseconds until the banner disappears (0 if not visible). */
uint32_t dhikr_banner_remaining_ms(const DhikrBanner* banner, uint32_t now_ms);

/* --- Applying an action ------------------------------------------------------ */

typedef struct {
    /** State changed and must be saved eventually. */
    bool changed;
    /** Save right away (completed round, undo). */
    bool save_now;
    bool play_feedback;
    DhikrFeedbackEvent feedback_event;
} DhikrActionOutcome;

/**
 * Apply one counter-screen action to the active profile and update the banner.
 *
 * Every OK press counts exactly once, also while "Set N complete!" is shown:
 * the press counts as the first of the next round and closes the banner
 * early, so no input is lost or counted twice. The target feedback is
 * requested exactly once per completed round.
 */
DhikrActionOutcome dhikr_counter_apply(
    DhikrState* state,
    DhikrBanner* banner,
    DhikrCounterAction action,
    uint32_t now_ms);

#ifdef __cplusplus
}
#endif
