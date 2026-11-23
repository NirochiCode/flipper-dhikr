/*
 * View Progress: read-only details of the profile chosen in the list
 * (app->edit_profile_id), which does not have to be the active one.
 *
 *        Istighfar
 *   Current: 15/100
 *   Completed Sets: 4
 *   Target: 100
 *
 * No key changes any data; Back returns to the details menu of the same profile.
 */
#include "include/dhikr_app.h"
#include "include/menu.h"

#include <string.h>

/* Longer names use the small font so they fit the 128 px width. */
#define PROGRESS_TITLE_PRIMARY_MAX_CHARS 13

void dhikr_scene_profile_progress_on_enter(void* context) {
    DhikrApp* app = context;
    widget_reset(app->widget);

    if(dhikr_app_resolve_edit_profile(app)) {
        const DhikrProfile* profile = &app->state.profiles[app->edit_profile];
        DhikrProgressText text;
        dhikr_progress_text(profile, &text);

        const Font title_font = strlen(profile->name) > PROGRESS_TITLE_PRIMARY_MAX_CHARS ?
                                    FontSecondary :
                                    FontPrimary;
        widget_add_string_element(
            app->widget, 64, 2, AlignCenter, AlignTop, title_font, profile->name);
        widget_add_string_element(
            app->widget, 4, 20, AlignLeft, AlignTop, FontSecondary, text.current);
        widget_add_string_element(app->widget, 4, 32, AlignLeft, AlignTop, FontSecondary, text.sets);
        widget_add_string_element(
            app->widget, 4, 44, AlignLeft, AlignTop, FontSecondary, text.target);
    } else {
        widget_add_string_element(
            app->widget, 64, 28, AlignCenter, AlignTop, FontSecondary, "Profile not found");
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewWidget);
}

bool dhikr_scene_profile_progress_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false; // Back: default handling returns to the details menu
}

void dhikr_scene_profile_progress_on_exit(void* context) {
    DhikrApp* app = context;
    widget_reset(app->widget);
}
