#include "include/dhikr_app.h"
#include "include/feedback_player.h"

typedef enum {
    FeedbackItemMode,
    FeedbackItemPreviewCount,
    FeedbackItemPreviewTarget,
} FeedbackItem;

static void feedback_mode_changed(VariableItem* item) {
    DhikrApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    if(!dhikr_feedback_mode_is_valid(index)) return;

    app->state.feedback = (DhikrFeedbackMode)index;
    variable_item_set_current_value_text(item, dhikr_feedback_mode_name(app->state.feedback));
    dhikr_app_mark_dirty(app);
    // Immediate preview of the selected mode.
    dhikr_feedback_play(app->notifications, app->state.feedback, DhikrFeedbackEventCount);
}

static void feedback_enter_callback(void* context, uint32_t index) {
    DhikrApp* app = context;
    if(index == FeedbackItemPreviewCount) {
        dhikr_feedback_play(app->notifications, app->state.feedback, DhikrFeedbackEventCount);
    } else if(index == FeedbackItemPreviewTarget) {
        dhikr_feedback_play(app->notifications, app->state.feedback, DhikrFeedbackEventTarget);
    }
}

void dhikr_scene_feedback_on_enter(void* context) {
    DhikrApp* app = context;
    VariableItemList* list = app->variable_item_list;

    variable_item_list_reset(list);
    VariableItem* item =
        variable_item_list_add(list, "Feedback", DhikrFeedbackModeCount, feedback_mode_changed, app);
    variable_item_set_current_value_index(item, app->state.feedback);
    variable_item_set_current_value_text(item, dhikr_feedback_mode_name(app->state.feedback));

    variable_item_list_add(list, "Preview count", 1, NULL, NULL);
    variable_item_list_add(list, "Preview target", 1, NULL, NULL);
    variable_item_list_set_enter_callback(list, feedback_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DhikrViewVariableItemList);
}

bool dhikr_scene_feedback_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void dhikr_scene_feedback_on_exit(void* context) {
    DhikrApp* app = context;
    variable_item_list_reset(app->variable_item_list);
}
