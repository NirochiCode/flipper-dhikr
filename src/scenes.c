#include "include/scenes.h"

#define DHIKR_SCENE_ON_ENTER(id, name) dhikr_scene_##name##_on_enter,
static void (*const dhikr_on_enter_handlers[])(void*) = {
    DHIKR_SCENE_LIST(DHIKR_SCENE_ON_ENTER)};
#undef DHIKR_SCENE_ON_ENTER

#define DHIKR_SCENE_ON_EVENT(id, name) dhikr_scene_##name##_on_event,
static bool (*const dhikr_on_event_handlers[])(void*, SceneManagerEvent) = {
    DHIKR_SCENE_LIST(DHIKR_SCENE_ON_EVENT)};
#undef DHIKR_SCENE_ON_EVENT

#define DHIKR_SCENE_ON_EXIT(id, name) dhikr_scene_##name##_on_exit,
static void (*const dhikr_on_exit_handlers[])(void*) = {DHIKR_SCENE_LIST(DHIKR_SCENE_ON_EXIT)};
#undef DHIKR_SCENE_ON_EXIT

const SceneManagerHandlers dhikr_scene_handlers = {
    .on_enter_handlers = dhikr_on_enter_handlers,
    .on_event_handlers = dhikr_on_event_handlers,
    .on_exit_handlers = dhikr_on_exit_handlers,
    .scene_num = DhikrSceneCount,
};
