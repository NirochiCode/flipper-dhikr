/**
 * @file scenes.h
 * @brief Scene list. Each entry X(Id, name) expects the handlers
 *        dhikr_scene_<name>_on_enter / _on_event / _on_exit in scene_<name>.c.
 *
 * To add a screen: add a line here and create the matching scene file.
 */
#pragma once

#include <gui/scene_manager.h>

#define DHIKR_SCENE_LIST(X)              \
    X(MainMenu, main_menu)               \
    X(Counter, counter)                  \
    X(Profiles, profiles)                \
    X(ProfileEdit, profile_edit)         \
    X(ProfileActions, profile_actions)   \
    X(ProfileProgress, profile_progress) \
    X(ProfileName, profile_name)         \
    X(ProfileDelete, profile_delete)     \
    X(Target, target)                    \
    X(TargetCustom, target_custom)       \
    X(Feedback, feedback)                \
    X(FeedbackEvent, feedback_event)     \
    X(ProfileReset, profile_reset)       \
    X(About, about)                      \
    X(Notice, notice)

#define DHIKR_SCENE_ENUM(id, name) DhikrScene##id,
typedef enum {
    DHIKR_SCENE_LIST(DHIKR_SCENE_ENUM) DhikrSceneCount,
} DhikrScene;
#undef DHIKR_SCENE_ENUM

#define DHIKR_SCENE_PROTOTYPES(id, name)                                        \
    void dhikr_scene_##name##_on_enter(void* context);                          \
    bool dhikr_scene_##name##_on_event(void* context, SceneManagerEvent event); \
    void dhikr_scene_##name##_on_exit(void* context);
DHIKR_SCENE_LIST(DHIKR_SCENE_PROTOTYPES)
#undef DHIKR_SCENE_PROTOTYPES

extern const SceneManagerHandlers dhikr_scene_handlers;
