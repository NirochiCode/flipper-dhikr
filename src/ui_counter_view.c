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
#define BAR_Y            39 /* elements_progress_bar_with_text is 11 px high */
#define BAR_WIDTH        120
#define HINT_BASELINE_Y  63 /* bottom row, shared with the center "+1" button */

struct DhikrCounterView {
    View* view;
    FuriTimer* banner_timer;
    DhikrCounterViewCallback callback;
    void* context;
};

typedef struct {
    char name[DHIKR_PROFILE_NAME_SIZE];
    DhikrCounter counter;
    uint8_t profile_count;
    DhikrBanner banner;
    bool muted;
} DhikrCounterViewModel;

uint32_t dhikr_ui_now_ms(void) {
    return (uint32_t)(((uint64_t)furi_get_tick() * 1000U) / furi_kernel_get_tick_frequency());
}

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

static void counter_view_draw_count(Canvas* canvas, uint32_t count, uint32_t target) {
    char count_text[12];
    char target_text[12];
    snprintf(count_text, sizeof(count_text), "%lu", (unsigned long)count);
    snprintf(target_text, sizeof(target_text), "/%lu", (unsigned long)target);

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

static void counter_view_draw_bottom(Canvas* canvas, const DhikrCounterViewModel* model) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    // Undo hint: small down arrow + "Undo" (hold Down).
    canvas_draw_triangle(canvas, 4, HINT_BASELINE_Y - 7, 7, 4, CanvasDirectionTopToBottom);
    canvas_draw_str(canvas, 10, HINT_BASELINE_Y, "Undo");

    if(model->muted) {
        canvas_draw_str_aligned(canvas, 127, HINT_BASELINE_Y, AlignRight, AlignBottom, "Muted");
    }

    elements_button_center(canvas, "+1");
}

static void counter_view_draw(Canvas* canvas, void* model_ptr) {
    const DhikrCounterViewModel* model = model_ptr;
    const DhikrCounter* counter = &model->counter;
    const uint32_t now = dhikr_ui_now_ms();
    const DhikrBannerKind banner =
        dhikr_banner_is_visible(&model->banner, now) ? model->banner.kind : DhikrBannerNone;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    counter_view_draw_name(canvas, model);

    char text[32];
    float progress;

    if(banner == DhikrBannerTarget) {
        // The round is already reset internally; show the completed round briefly.
        counter_view_draw_count(canvas, model->banner.value, model->banner.value);
        snprintf(text, sizeof(text), "Target reached!");
        progress = 1.0f;
    } else {
        counter_view_draw_count(canvas, counter->count, counter->target);
        switch(banner) {
        case DhikrBannerUndo:
            snprintf(text, sizeof(text), "Undo: -1");
            break;
        case DhikrBannerRoundRestored:
            snprintf(text, sizeof(text), "Round undone");
            break;
        case DhikrBannerNothingToUndo:
            snprintf(text, sizeof(text), "Nothing to undo");
            break;
        default:
            if(counter->rounds > 0) {
                snprintf(
                    text,
                    sizeof(text),
                    "%lu%%  Rounds: %lu",
                    (unsigned long)(dhikr_counter_progress_permille(counter) / 10),
                    (unsigned long)counter->rounds);
            } else {
                snprintf(
                    text,
                    sizeof(text),
                    "%lu%%",
                    (unsigned long)(dhikr_counter_progress_permille(counter) / 10));
            }
            break;
        }
        progress = (float)dhikr_counter_progress_permille(counter) / 1000.0f;
    }

    if(progress < 0.0f) progress = 0.0f;
    if(progress > 1.0f) progress = 1.0f;
    elements_progress_bar_with_text(canvas, BAR_X, BAR_Y, BAR_WIDTH, progress, text);

    counter_view_draw_bottom(canvas, model);
}

static DhikrKey map_key(InputKey key) {
    switch(key) {
    case InputKeyUp:
        return DhikrKeyUp;
    case InputKeyDown:
        return DhikrKeyDown;
    case InputKeyRight:
        return DhikrKeyRight;
    case InputKeyLeft:
        return DhikrKeyLeft;
    case InputKeyOk:
        return DhikrKeyOk;
    case InputKeyBack:
        return DhikrKeyBack;
    default:
        return DhikrKeyOther;
    }
}

static DhikrPress map_press(InputType type) {
    switch(type) {
    case InputTypePress:
        return DhikrPressPress;
    case InputTypeRelease:
        return DhikrPressRelease;
    case InputTypeShort:
        return DhikrPressShort;
    case InputTypeLong:
        return DhikrPressLong;
    case InputTypeRepeat:
        return DhikrPressRepeat;
    default:
        return DhikrPressOther;
    }
}

static bool counter_view_input(InputEvent* event, void* context) {
    DhikrCounterView* counter_view = context;

    // Back is handled by the view dispatcher (navigation to the menu).
    if(event->key == InputKeyBack) return false;

    DhikrCounterAction action = dhikr_counter_input_map(map_key(event->key), map_press(event->type));
    if(action != DhikrCounterActionNone && action != DhikrCounterActionExit &&
       counter_view->callback != NULL) {
        counter_view->callback(action, counter_view->context);
    }
    return true;
}

static void counter_view_banner_timer_callback(void* context) {
    DhikrCounterView* counter_view = context;
    // Redraw once the banner has expired (drawing decides visibility by time).
    with_view_model(counter_view->view, DhikrCounterViewModel * model, { UNUSED(model); }, true);
}

DhikrCounterView* dhikr_counter_view_alloc(void) {
    DhikrCounterView* counter_view = malloc(sizeof(DhikrCounterView));
    furi_check(counter_view); // never NULL on Flipper; stated for static analyzers
    counter_view->view = view_alloc();
    counter_view->callback = NULL;
    counter_view->context = NULL;
    counter_view->banner_timer =
        furi_timer_alloc(counter_view_banner_timer_callback, FuriTimerTypeOnce, counter_view);

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
    furi_timer_stop(counter_view->banner_timer);
    furi_timer_free(counter_view->banner_timer);
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
    uint8_t profile_count,
    const DhikrBanner* banner,
    bool muted) {
    furi_check(counter_view);
    furi_check(profile);

    DhikrBanner banner_copy = {0};
    if(banner != NULL) banner_copy = *banner;

    with_view_model(
        counter_view->view,
        DhikrCounterViewModel * model,
        {
            snprintf(model->name, sizeof(model->name), "%s", profile->name);
            model->counter = profile->counter;
            model->profile_count = profile_count;
            model->banner = banner_copy;
            model->muted = muted;
        },
        true);

    uint32_t remaining = dhikr_banner_remaining_ms(&banner_copy, dhikr_ui_now_ms());
    if(remaining > 0) {
        // furi_timer_start restarts a running timer.
        furi_timer_start(counter_view->banner_timer, furi_ms_to_ticks(remaining + 10));
    } else {
        furi_timer_stop(counter_view->banner_timer);
    }
}
