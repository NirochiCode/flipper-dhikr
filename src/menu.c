#include "include/menu.h"

#include <stdio.h>

const char* dhikr_main_menu_label(DhikrMainMenuItem item) {
    switch(item) {
    case DhikrMainMenuStartCounting:
        return "Start Counting";
    case DhikrMainMenuProfiles:
        return "Dhikr Profiles";
    case DhikrMainMenuFeedback:
        return "Feedback Settings";
    case DhikrMainMenuReset:
        return "Reset Counter";
    case DhikrMainMenuAbout:
        return "About";
    case DhikrMainMenuCount:
    default:
        return "";
    }
}

bool dhikr_profile_action_allowed(const DhikrProfile* profile, DhikrProfileAction action) {
    if(profile == NULL) return false;
    switch(action) {
    case DhikrProfileActionTarget:
        return true;
    case DhikrProfileActionRename:
    case DhikrProfileActionDelete:
        // Built-in profiles keep their names and cannot be deleted.
        return !profile->builtin;
    case DhikrProfileActionCount:
    default:
        return false;
    }
}

uint8_t dhikr_profile_actions(const DhikrProfile* profile, DhikrProfileAction out[DhikrProfileActionCount]) {
    uint8_t count = 0;
    if(out == NULL) return 0;
    for(int action = 0; action < DhikrProfileActionCount; action++) {
        if(dhikr_profile_action_allowed(profile, (DhikrProfileAction)action)) {
            out[count++] = (DhikrProfileAction)action;
        }
    }
    return count;
}

void dhikr_profile_action_label(
    const DhikrProfile* profile,
    DhikrProfileAction action,
    char* out,
    size_t out_size) {
    if(out == NULL || out_size == 0) return;
    switch(action) {
    case DhikrProfileActionTarget:
        if(profile != NULL) {
            snprintf(out, out_size, "Edit Target (%lu)", (unsigned long)profile->counter.target);
        } else {
            snprintf(out, out_size, "Edit Target");
        }
        break;
    case DhikrProfileActionRename:
        snprintf(out, out_size, "Rename Profile");
        break;
    case DhikrProfileActionDelete:
        snprintf(out, out_size, "Delete Profile");
        break;
    case DhikrProfileActionCount:
    default:
        out[0] = '\0';
        break;
    }
}
