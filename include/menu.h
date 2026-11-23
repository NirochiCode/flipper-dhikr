/**
 * @file menu.h
 * @brief Hardware-independent menu structure (entries and their labels), so the
 *        menu layout can be checked by the host unit tests. The scenes map the
 *        entries to screens.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "include/profiles.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Main menu --------------------------------------------------------------- */

/**
 * Main menu entries in display order. Targets and resets are per profile
 * (Dhikr Profiles -> profile details), so they have no main menu entry.
 */
typedef enum {
    DhikrMainMenuStartCounting,
    DhikrMainMenuProfiles,
    DhikrMainMenuFeedback,
    DhikrMainMenuAbout,
    DhikrMainMenuCount,
} DhikrMainMenuItem;

/** Label of a main menu entry ("" for an invalid entry). */
const char* dhikr_main_menu_label(DhikrMainMenuItem item);

/* --- Profile edit menu ------------------------------------------------------- */

/** Profile details actions, in display order. */
typedef enum {
    DhikrProfileActionProgress,
    DhikrProfileActionTarget,
    DhikrProfileActionRename,
    DhikrProfileActionReset,
    DhikrProfileActionDelete,
    DhikrProfileActionCount,
} DhikrProfileAction;

/**
 * Actions offered for a profile, in display order. Every profile offers
 * View Progress, Edit Target and Reset Progress; custom profiles also Rename
 * and Delete.
 * @return number of actions written to `out` (capacity DhikrProfileActionCount)
 */
uint8_t dhikr_profile_actions(const DhikrProfile* profile, DhikrProfileAction out[DhikrProfileActionCount]);

/** Whether `action` is allowed for `profile`. */
bool dhikr_profile_action_allowed(const DhikrProfile* profile, DhikrProfileAction action);

/**
 * Menu label of an action. Edit Target includes the profile's current target,
 * e.g. "Edit Target (33)".
 */
void dhikr_profile_action_label(
    const DhikrProfile* profile,
    DhikrProfileAction action,
    char* out,
    size_t out_size);

/* --- Edit Profiles list ----------------------------------------------------- */

/**
 * Label of a profile in the "Edit Profiles" list: only the profile name, for
 * built-in and custom profiles alike (no suffix). Truncated to `out_size`.
 */
void dhikr_profile_list_label(const DhikrProfile* profile, char* out, size_t out_size);

/* --- View Progress ----------------------------------------------------------- */

#define DHIKR_PROGRESS_LINE_SIZE 32

/** Read-only lines of the View Progress screen (the name is shown as the title). */
typedef struct {
    char current[DHIKR_PROGRESS_LINE_SIZE]; /**< "Current: 21/33" */
    char sets[DHIKR_PROGRESS_LINE_SIZE]; /**< "Completed Sets: 3" */
    char target[DHIKR_PROGRESS_LINE_SIZE]; /**< "Target: 33" */
} DhikrProgressText;

/** Fill the View Progress lines from `profile` (reads only). */
void dhikr_progress_text(const DhikrProfile* profile, DhikrProgressText* out);

#ifdef __cplusplus
}
#endif
