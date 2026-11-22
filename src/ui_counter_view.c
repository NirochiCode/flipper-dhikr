#include "include/ui.h"

#include <furi.h>
#include <gui/elements.h>
#include <stdio.h>
#include <string.h>

/* Layout (128 x 64) */
#define NAME_Y           1
#define NAME_MAX_WIDTH   110
#define COUNT_BASELINE_Y 35
#define BAR_X            4
#define BAR_Y            39
#define BAR_WIDTH        120

struct DhikrCounterView {
    View* view;
    DhikrCounterViewCallback callback;
    void* context;
};

typedef struct {
    char name[DHIKR_PROFILE_NAME_SIZE];
    DhikrCounter counter;
    uint8_t profile_count;
} DhikrCounterViewModel;

static void counter_view_draw_name(Canvas* canvas, const DhikrCounterViewModel* model) {
    canvas_set_font(canvas, FontPrimary);
    if(canvas_string_width(canvas, model->name) > NAME_MAX_WIDTH) {
        // Long custom names fall back to the narrower font instead of being cut.
        canvas_set_font(canvas, FontSecondary);
    }
    canvas_draw_str_aligned(canvas, 64, NAME_Y, AlignCenter, AlignTop, model->name);

    if(model->profile_count > 1) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 1, NAME_Y + 1, AlignLeft, AlignTop, "<");
        canvas_draw_str_aligned(canvas, 127, NAME_Y + 1, AlignRight, AlignTop, ">");
    }
}

static void counter_view_draw_count(Canvas* canvas, const DhikrCounterViewModel* model) {
    char count_text[12];
    char target_text[12];
    snprintf(count_text, sizeof(count_text), "%lu", (unsigned long)model->counter.count);
    snprintf(target_text, sizeof(target_text), "/%lu", (unsigned long)model->counter.target);

    canvas_set_font(canvas, FontBigNumbers);
    const int32_t count_width = canvas_string_width(canvas, count_text);
    canvas_set_font(canvas, FontSecondary);
    const int32_t target_width = canvas_string_width(canvas, target_text);

    const int32_t gap = 2;
    int32_t x = (128 - (count_width + gap + target_width)) / 2;
    if(x < 0) x = 0;

    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, x, COUNT_BASELINE_Y, count_text);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x + count_width + gap, COUNT_BASELINE_Y, target_text);
}

static void counter_view_draw_progress(Canvas* canvas, const DhikrCounterViewModel* model) {
    const DhikrCounter* counter = &model->counter;
    char text[24];

    if(dhikr_counter_is_saturated(counter)) {
        snprintf(text, sizeof(text), "Maximum reached");
    } else if(counter->count == counter->target) {
        snprintf(text, sizeof(text), "Target reached!");
    } else if(dhikr_counter_is_complete(counter)) {
        snprintf(text, sizeof(text), "Done +%lu", (unsigned long)(counter->count - counter->target));
    } else {
        snprintf(
            text,
            sizeof(text),
            "%lu%%",
            (unsigned long)(dhikr_counter_progress_permille(counter) / 10));
    }

    float progress = (float)dhikr_counter_progress_permille(counter) / 1000.0f;
    if(progress < 0.0f) progress = 0.0f;
    if(progress > 1.0f) progress = 1.0f;
    elements_progress_bar_with_text(canvas, BAR_X, BAR_Y, BAR_WIDTH, progress, text);
}

static void counter_view_draw(Canvas* canvas, void* model_ptr) {
    const DhikrCounterViewModel* model = model_ptr;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    counter_view_draw_name(canvas, model);
    counter_view_draw_count(canvas, model);
    counter_view_draw_progress(canvas, model);

    canvas_set_color(canvas, ColorBlack);
    elements_button_center(canvas, "+1");
}

static bool counter_view_input(InputEvent* event, void* context) {
    DhikrCounterView* counter_view = context;

    // Back is handled by the view dispatcher (navigation to the menu).
    if(event->key == InputKeyBack) return false;
    // Only complete short presses count: no repeats, no long-press side effects.
    if(event->type != InputTypeShort) return true;
    if(counter_view->callback == NULL) return true;

    switch(event->key) {
    case InputKeyOk:
        counter_view->callback(DhikrCounterViewEventIncrement, counter_view->context);
        break;
    case InputKeyLeft:
        counter_view->callback(DhikrCounterViewEventPrevProfile, counter_view->context);
        break;
    case InputKeyRight:
        counter_view->callback(DhikrCounterViewEventNextProfile, counter_view->context);
        break;
    default:
        break;
    }
    return true;
}

DhikrCounterView* dhikr_counter_view_alloc(void) {
    DhikrCounterView* counter_view = malloc(sizeof(DhikrCounterView));
    counter_view->view = view_alloc();
    counter_view->callback = NULL;
    counter_view->context = NULL;

    view_allocate_model(counter_view->view, ViewModelTypeLocking, sizeof(DhikrCounterViewModel));
    view_set_context(counter_view->view, counter_view);
    view_set_draw_callback(counter_view->view, counter_view_draw);
    view_set_input_callback(counter_view->view, counter_view_input);

    with_view_model(
        counter_view->view,
        DhikrCounterViewModel * model,
        {
            memset(model, 0, sizeof(*model));
            model->counter.target = DHIKR_TARGET_DEFAULT;
        },
        false);
    return counter_view;
}

void dhikr_counter_view_free(DhikrCounterView* counter_view) {
    furi_check(counter_view);
    view_free(counter_view->view);
    free(counter_view);
}

View* dhikr_counter_view_get_view(DhikrCounterView* counter_view) {
    furi_check(counter_view);
    return counter_view->view;
}

void dhikr_counter_view_set_callback(
    DhikrCounterView* counter_view,
    DhikrCounterViewCallback callback,
    void* context) {
    furi_check(counter_view);
    counter_view->callback = callback;
    counter_view->context = context;
}

void dhikr_counter_view_update(
    DhikrCounterView* counter_view,
    const DhikrProfile* profile,
    uint8_t profile_count) {
    furi_check(counter_view);
    furi_check(profile);
    with_view_model(
        counter_view->view,
        DhikrCounterViewModel * model,
        {
            snprintf(model->name, sizeof(model->name), "%s", profile->name);
            model->counter = profile->counter;
            model->profile_count = profile_count;
        },
        true);
}
