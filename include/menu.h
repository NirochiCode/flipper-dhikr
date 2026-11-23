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

/** Main menu entries in display order. Targets are edited per profile (Dhikr Profiles). */
typedef enum {
    DhikrMainMenuStartCounting,
    DhikrMainMenuProfiles,
    DhikrMainMenuFeedback,
    DhikrMainMenuReset,
    DhikrMainMenuAbout,
    DhikrMainMenuCount,
} DhikrMainMenuItem;

/** Label of a main menu entry ("" for an invalid entry). */
const char* dhikr_main_menu_label(DhikrMainMenuItem item);

/* --- Profile edit menu ------------------------------------------------------- */

typedef enum {
    DhikrProfileActionTarget,
    DhikrProfileActionRename,
    DhikrProfileActionDelete,
    DhikrProfileActionCount,
} DhikrProfileAction;

/**
 * Actions offered for a profile, in display order. Built-in profiles only offer
 * Edit Target; custom profiles also Rename and Delete.
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

#ifdef __cplusplus
}
#endif
