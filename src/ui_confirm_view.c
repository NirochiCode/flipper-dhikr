#include "include/ui.h"

#include <furi.h>
#include <gui/elements.h>
#include <stdio.h>

#include "include/confirm.h"

#define CONFIRM_TEXT_SIZE 40
#define BUTTON_WIDTH      40
#define BUTTON_HEIGHT     13
#define BUTTON_Y          48
#define BUTTON_NO_X       18
#define BUTTON_YES_X      70

struct DhikrConfirmView {
    View* view;
    DhikrConfirmViewCallback callback;
    void* context;
};

typedef struct {
    DhikrConfirm confirm;
    char title[CONFIRM_TEXT_SIZE];
    char line1[CONFIRM_TEXT_SIZE];
    char line2[CONFIRM_TEXT_SIZE];
} DhikrConfirmViewModel;

static void confirm_view_draw_button(Canvas* canvas, int32_t x, const char* label, bool selected) {
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        canvas_draw_rbox(canvas, x, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, 3);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, 3);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, x + BUTTON_WIDTH / 2, BUTTON_Y + BUTTON_HEIGHT / 2 + 1, AlignCenter, AlignCenter, label);
    canvas_set_color(canvas, ColorBlack);
}

static void confirm_view_draw(Canvas* canvas, void* model_ptr) {
    const DhikrConfirmViewModel* model = model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, model->title);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, model->line1);
    canvas_draw_str_aligned(canvas, 64, 29, AlignCenter, AlignTop, model->line2);

    const bool yes = model->confirm.selection == DhikrConfirmChoiceYes;
    confirm_view_draw_button(canvas, BUTTON_NO_X, "No", !yes);
    confirm_view_draw_button(canvas, BUTTON_YES_X, "Yes", yes);
}

static bool confirm_view_input(InputEvent* event, void* context) {
    DhikrConfirmView* confirm_view = context;
    if(event->type != InputTypeShort) return true;

    DhikrConfirmInput input;
    switch(event->key) {
    case InputKeyLeft:
        input = DhikrConfirmInputLeft;
        break;
    case InputKeyRight:
        input = DhikrConfirmInputRight;
        break;
    case InputKeyOk:
        input = DhikrConfirmInputOk;
        break;
    case InputKeyBack:
        input = DhikrConfirmInputBack;
        break;
    default:
        return true;
    }

    DhikrConfirmOutcome outcome = DhikrConfirmPending;
    with_view_model(
        confirm_view->view,
        DhikrConfirmViewModel * model,
        { outcome = dhikr_confirm_handle(&model->confirm, input); },
        true);

    if(outcome != DhikrConfirmPending && confirm_view->callback != NULL) {
        confirm_view->callback(
            outcome == DhikrConfirmAccepted ? DhikrConfirmViewEventAccepted :
                                              DhikrConfirmViewEventCancelled,
            confirm_view->context);
    }
    return true;
}

DhikrConfirmView* dhikr_confirm_view_alloc(void) {
    DhikrConfirmView* confirm_view = malloc(sizeof(DhikrConfirmView));
    confirm_view->view = view_alloc();
    confirm_view->callback = NULL;
    confirm_view->context = NULL;

    view_allocate_model(confirm_view->view, ViewModelTypeLocking, sizeof(DhikrConfirmViewModel));
    view_set_context(confirm_view->view, confirm_view);
    view_set_draw_callback(confirm_view->view, confirm_view_draw);
    view_set_input_callback(confirm_view->view, confirm_view_input);

    dhikr_confirm_view_setup(confirm_view, "", "", "");
    return confirm_view;
}

void dhikr_confirm_view_free(DhikrConfirmView* confirm_view) {
    furi_check(confirm_view);
    view_free(confirm_view->view);
    free(confirm_view);
}

View* dhikr_confirm_view_get_view(DhikrConfirmView* confirm_view) {
    furi_check(confirm_view);
    return confirm_view->view;
}

void dhikr_confirm_view_set_callback(
    DhikrConfirmView* confirm_view,
    DhikrConfirmViewCallback callback,
    void* context) {
    furi_check(confirm_view);
    confirm_view->callback = callback;
    confirm_view->context = context;
}

void dhikr_confirm_view_setup(
    DhikrConfirmView* confirm_view,
    const char* title,
    const char* line1,
    const char* line2) {
    furi_check(confirm_view);
    with_view_model(
        confirm_view->view,
        DhikrConfirmViewModel * model,
        {
            dhikr_confirm_begin(&model->confirm);
            snprintf(model->title, sizeof(model->title), "%s", title ? title : "");
            snprintf(model->line1, sizeof(model->line1), "%s", line1 ? line1 : "");
            snprintf(model->line2, sizeof(model->line2), "%s", line2 ? line2 : "");
        },
        true);
}
