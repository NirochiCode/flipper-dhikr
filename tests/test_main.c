/*
 * Host unit tests for the hardware-independent Flipper Dhikr logic.
 * Build and run with `make -C tests` (see tests/Makefile).
 *
 * Tests marked [R<n>] cover requirement n of the v1.0.1 test list, [F<n>] the v1.0.2
 * feedback list, [U<n>] the v1.0.3 menu/UX list and [P<n>] the v1.0.3 profile
 * details / reset list.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "include/confirm.h"
#include "include/counter.h"
#include "include/counter_input.h"
#include "include/feedback.h"
#include "include/menu.h"
#include "include/profiles.h"
#include "include/settings_codec.h"
#include "include/storage.h"
#include "test_framework.h"

int g_tests_run = 0;
int g_tests_failed = 0;
int g_current_failed = 0;

/* ------------------------------------------------------------------------- */
/* In-memory file system fake                                                */
/* ------------------------------------------------------------------------- */

#define FAKE_FILES     6
#define FAKE_FILE_SIZE 8192

typedef struct {
    char path[64];
    char data[FAKE_FILE_SIZE];
    size_t length;
    bool exists;
} FakeFile;

typedef struct {
    FakeFile files[FAKE_FILES];
    bool read_error;
    bool write_error;
    bool rename_error;
    int write_count;
} FakeFs;

static FakeFile* fake_find(FakeFs* fs, const char* path) {
    for(int i = 0; i < FAKE_FILES; i++) {
        if(fs->files[i].exists && strcmp(fs->files[i].path, path) == 0) return &fs->files[i];
    }
    return NULL;
}

static FakeFile* fake_create(FakeFs* fs, const char* path) {
    FakeFile* file = fake_find(fs, path);
    if(file) return file;
    for(int i = 0; i < FAKE_FILES; i++) {
        if(!fs->files[i].exists) {
            file = &fs->files[i];
            memset(file, 0, sizeof(*file));
            strncpy(file->path, path, sizeof(file->path) - 1);
            file->exists = true;
            return file;
        }
    }
    return NULL;
}

static void fake_put(FakeFs* fs, const char* path, const char* data, size_t length) {
    FakeFile* file = fake_create(fs, path);
    memcpy(file->data, data, length);
    file->length = length;
}

static DhikrIoStatus
    fake_read(void* ctx, const char* path, char* buffer, size_t capacity, size_t* length) {
    FakeFs* fs = ctx;
    if(fs->read_error) return DhikrIoError;
    FakeFile* file = fake_find(fs, path);
    if(!file) return DhikrIoNotFound;
    size_t n = file->length < capacity ? file->length : capacity;
    memcpy(buffer, file->data, n);
    *length = n;
    return DhikrIoOk;
}

static DhikrIoStatus fake_write(void* ctx, const char* path, const char* data, size_t length) {
    FakeFs* fs = ctx;
    if(fs->write_error || length > FAKE_FILE_SIZE) return DhikrIoError;
    fs->write_count++;
    fake_put(fs, path, data, length);
    return DhikrIoOk;
}

static DhikrIoStatus fake_rename(void* ctx, const char* from, const char* to) {
    FakeFs* fs = ctx;
    if(fs->rename_error) return DhikrIoError;
    FakeFile* src = fake_find(fs, from);
    if(!src) return DhikrIoNotFound;
    FakeFile* dst = fake_find(fs, to);
    if(dst) dst->exists = false;
    strncpy(src->path, to, sizeof(src->path) - 1);
    return DhikrIoOk;
}

static DhikrIoStatus fake_remove(void* ctx, const char* path) {
    FakeFs* fs = ctx;
    FakeFile* file = fake_find(fs, path);
    if(!file) return DhikrIoNotFound;
    file->exists = false;
    return DhikrIoOk;
}

static FakeFs g_fs;
static DhikrFileIo g_io;

static void fake_reset(void) {
    memset(&g_fs, 0, sizeof(g_fs));
    g_io = (DhikrFileIo){
        .context = &g_fs,
        .read = fake_read,
        .write = fake_write,
        .rename = fake_rename,
        .remove = fake_remove,
    };
}

static bool feedback_equal(const DhikrFeedbackSettings* a, const DhikrFeedbackSettings* b) {
    return a->counter_vibration == b->counter_vibration && a->counter_sound == b->counter_sound &&
           a->target_vibration == b->target_vibration && a->target_sound == b->target_sound;
}

static DhikrFeedbackSettings fb(bool cv, bool cs, bool tv, bool ts) {
    DhikrFeedbackSettings settings = {
        .counter_vibration = cv, .counter_sound = cs, .target_vibration = tv, .target_sound = ts};
    return settings;
}

static bool states_equal(const DhikrState* a, const DhikrState* b) {
    if(a->profile_count != b->profile_count || a->active != b->active ||
       !feedback_equal(&a->feedback, &b->feedback) || a->next_custom_id != b->next_custom_id) {
        return false;
    }
    for(uint8_t i = 0; i < a->profile_count; i++) {
        const DhikrProfile* pa = &a->profiles[i];
        const DhikrProfile* pb = &b->profiles[i];
        if(strcmp(pa->id, pb->id) != 0 || strcmp(pa->name, pb->name) != 0 ||
           pa->counter.count != pb->counter.count || pa->counter.target != pb->counter.target ||
           pa->counter.rounds != pb->counter.rounds || pa->builtin != pb->builtin) {
            return false;
        }
    }
    return true;
}

/** Save `s` with a fresh store and load it back with another fresh store. */
static DhikrLoadResult save_and_reload(const DhikrState* s, DhikrState* loaded) {
    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_save(&store, s), DhikrSaveWritten);
    DhikrStore store2;
    dhikr_store_init(&store2, &g_io);
    return dhikr_store_load(&store2, loaded);
}

/* Build a file with a correct checksum around an arbitrary body. */
static size_t build_file(char* out, size_t capacity, const char* body) {
    int n = snprintf(out, capacity, "%s", body);
    uint32_t crc = dhikr_crc32(out, (size_t)n);
    n += snprintf(out + n, capacity - (size_t)n, "crc32=%08lX\n", (unsigned long)crc);
    return (size_t)n;
}

/* ========================================================================= */
/* Counter basics (v1.0 behaviour that must still work)                      */
/* ========================================================================= */

static void test_increment_from_zero_to_one(void) {
    DhikrCounter c;
    dhikr_counter_init(&c, 33);
    EXPECT_EQ_U(c.count, 0);
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementCounted);
    EXPECT_EQ_U(c.count, 1);
    EXPECT_EQ_U(c.target, 33);
    EXPECT_EQ_U(c.rounds, 0);
}

static void test_custom_target(void) {
    DhikrCounter c;
    dhikr_counter_init(&c, 33);
    EXPECT(dhikr_counter_set_target(&c, 7));
    EXPECT_EQ_U(c.target, 7);
    for(int i = 0; i < 6; i++)
        EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementCounted);
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementRoundCompleted);
    EXPECT(dhikr_counter_set_target(&c, DHIKR_TARGET_MAX));
}

static void test_invalid_targets_rejected(void) {
    DhikrCounter c;
    dhikr_counter_init(&c, 50);
    c.count = 12;
    EXPECT(!dhikr_counter_set_target(&c, 0));
    EXPECT(!dhikr_counter_set_target(&c, DHIKR_TARGET_MAX + 1));
    EXPECT(!dhikr_counter_set_target(&c, UINT32_MAX));
    EXPECT(!dhikr_counter_set_target(NULL, 10));
    EXPECT_EQ_U(c.target, 50);
    EXPECT_EQ_U(c.count, 12);
    EXPECT(!dhikr_target_is_valid(0));
    EXPECT(dhikr_target_is_valid(1));
    EXPECT(dhikr_target_is_valid(9999));
    EXPECT(!dhikr_target_is_valid(10000));

    DhikrCounter d;
    dhikr_counter_init(&d, 0); // invalid -> default
    EXPECT_EQ_U(d.target, DHIKR_TARGET_DEFAULT);
}

static void test_target_change_keeps_count(void) {
    DhikrCounter c;
    dhikr_counter_init(&c, 33);
    for(int i = 0; i < 20; i++)
        dhikr_counter_increment(&c);
    EXPECT(dhikr_counter_set_target(&c, 99));
    EXPECT_EQ_U(c.count, 20);
    // Lowering the target below the count keeps the count ...
    EXPECT(dhikr_counter_set_target(&c, 10));
    EXPECT_EQ_U(c.count, 20);
    // ... and the next count completes the round.
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementRoundCompleted);
    EXPECT_EQ_U(c.count, 0);
    EXPECT_EQ_U(c.rounds, 1);
}

static void test_progress(void) {
    DhikrCounter c;
    dhikr_counter_init(&c, 3);
    EXPECT_EQ_U(dhikr_counter_progress_permille(&c), 0);
    dhikr_counter_increment(&c);
    EXPECT_EQ_U(dhikr_counter_progress_permille(&c), 333);
    c.target = 9999;
    c.count = 9998;
    EXPECT_EQ_U(dhikr_counter_progress_permille(&c), 999);
}

static void test_counter_never_overflows(void) {
    DhikrCounter c;
    dhikr_counter_init(&c, 33);
    // Largest count that can come from old data: the next press completes a round.
    c.count = DHIKR_COUNT_MAX;
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementRoundCompleted);
    EXPECT_EQ_U(c.count, 0);
    // Rounds saturate.
    c.rounds = DHIKR_ROUNDS_MAX;
    c.count = 32;
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementRoundCompleted);
    EXPECT_EQ_U(c.rounds, DHIKR_ROUNDS_MAX);
    // Garbage in memory is never incremented past the target.
    c.count = UINT32_MAX;
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementRoundCompleted);
    EXPECT_EQ_U(c.count, 0);
    EXPECT(dhikr_counter_is_valid(&c));
}

/* ========================================================================= */
/* [R1-R3] Undo                                                              */
/* ========================================================================= */

static void test_undo_at_ten(void) { // [R1]
    DhikrCounter c;
    dhikr_counter_init(&c, 33);
    for(int i = 0; i < 10; i++)
        dhikr_counter_increment(&c);
    EXPECT_EQ_U(dhikr_counter_undo(&c), DhikrUndoDecremented);
    EXPECT_EQ_U(c.count, 9);
    EXPECT_EQ_U(c.rounds, 0);
    EXPECT_EQ_U(c.target, 33);
}

static void test_undo_at_zero(void) { // [R2]
    DhikrCounter c;
    dhikr_counter_init(&c, 33);
    EXPECT_EQ_U(dhikr_counter_undo(&c), DhikrUndoNothing);
    EXPECT_EQ_U(c.count, 0);
    EXPECT_EQ_U(c.rounds, 0);
    // Count back down to 0 and further: never below 0.
    dhikr_counter_increment(&c);
    EXPECT_EQ_U(dhikr_counter_undo(&c), DhikrUndoDecremented);
    EXPECT_EQ_U(dhikr_counter_undo(&c), DhikrUndoNothing);
    EXPECT_EQ_U(c.count, 0);
}

static void test_long_down_undoes_once(void) { // [R3]
    // A long hold of Down produces Press, Long, many Repeats, Release.
    const DhikrPress hold[] = {
        DhikrPressPress,
        DhikrPressLong,
        DhikrPressRepeat,
        DhikrPressRepeat,
        DhikrPressRepeat,
        DhikrPressRepeat,
        DhikrPressRelease,
    };
    DhikrState s;
    dhikr_state_init_defaults(&s);
    DhikrBanner banner = {0};
    dhikr_state_active(&s)->counter.count = 10;

    int undo_actions = 0;
    for(size_t i = 0; i < sizeof(hold) / sizeof(hold[0]); i++) {
        DhikrCounterAction action = dhikr_counter_input_map(DhikrKeyDown, hold[i]);
        if(action == DhikrCounterActionUndo) undo_actions++;
        dhikr_counter_apply(&s, &banner, action, 0);
    }
    EXPECT_EQ_U(undo_actions, 1);
    EXPECT_EQ_U(dhikr_state_active(&s)->counter.count, 9);

    // Other key mappings: unchanged navigation, nothing else triggers undo/count.
    EXPECT_EQ_U(dhikr_counter_input_map(DhikrKeyDown, DhikrPressShort), DhikrCounterActionNone);
    EXPECT_EQ_U(dhikr_counter_input_map(DhikrKeyOk, DhikrPressShort), DhikrCounterActionIncrement);
    EXPECT_EQ_U(dhikr_counter_input_map(DhikrKeyOk, DhikrPressLong), DhikrCounterActionNone);
    EXPECT_EQ_U(dhikr_counter_input_map(DhikrKeyOk, DhikrPressRepeat), DhikrCounterActionNone);
    EXPECT_EQ_U(dhikr_counter_input_map(DhikrKeyOk, DhikrPressPress), DhikrCounterActionNone);
    EXPECT_EQ_U(
        dhikr_counter_input_map(DhikrKeyLeft, DhikrPressShort), DhikrCounterActionPrevProfile);
    EXPECT_EQ_U(
        dhikr_counter_input_map(DhikrKeyRight, DhikrPressShort), DhikrCounterActionNextProfile);
    EXPECT_EQ_U(dhikr_counter_input_map(DhikrKeyBack, DhikrPressShort), DhikrCounterActionExit);
    EXPECT_EQ_U(dhikr_counter_input_map(DhikrKeyUp, DhikrPressLong), DhikrCounterActionNone);
}

static void test_undo_after_completed_round(void) {
    DhikrCounter c;
    dhikr_counter_init(&c, 33);
    c.count = 32;
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementRoundCompleted);
    EXPECT_EQ_U(c.count, 0);
    EXPECT_EQ_U(c.rounds, 1);
    // Undo directly after the completion restores 32 and removes the round.
    EXPECT_EQ_U(dhikr_counter_undo(&c), DhikrUndoRoundRestored);
    EXPECT_EQ_U(c.count, 32);
    EXPECT_EQ_U(c.rounds, 0);
    // Further undos count down normally.
    EXPECT_EQ_U(dhikr_counter_undo(&c), DhikrUndoDecremented);
    EXPECT_EQ_U(c.count, 31);
    // Counting again completes the round again (and announces it again).
    c.count = 32;
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementRoundCompleted);

    // After the next count the completed round can no longer be taken back.
    dhikr_counter_increment(&c); // 1
    EXPECT_EQ_U(dhikr_counter_undo(&c), DhikrUndoDecremented); // 0
    EXPECT_EQ_U(dhikr_counter_undo(&c), DhikrUndoNothing);
    EXPECT_EQ_U(c.rounds, 1);

    // A target change or manual reset also ends the undo window.
    c.count = 32;
    dhikr_counter_increment(&c);
    dhikr_counter_set_target(&c, 50);
    EXPECT_EQ_U(dhikr_counter_undo(&c), DhikrUndoNothing);

    // With a lowered target the exact previous count is restored.
    DhikrCounter d;
    dhikr_counter_init(&d, 33);
    d.count = 20;
    dhikr_counter_set_target(&d, 10);
    EXPECT_EQ_U(dhikr_counter_increment(&d), DhikrIncrementRoundCompleted);
    EXPECT_EQ_U(dhikr_counter_undo(&d), DhikrUndoRoundRestored);
    EXPECT_EQ_U(d.count, 20);
}

static void test_undo_only_active_profile_and_saved(void) {
    fake_reset();
    DhikrState s, loaded;
    dhikr_state_init_defaults(&s);
    s.profiles[0].counter.count = 5;
    s.profiles[1].counter.count = 7;
    dhikr_state_select(&s, 1);
    DhikrBanner banner = {0};
    DhikrActionOutcome o = dhikr_counter_apply(&s, &banner, DhikrCounterActionUndo, 0);
    EXPECT(o.changed && o.save_now && !o.play_feedback);
    EXPECT_EQ_U(banner.kind, DhikrBannerUndo);
    EXPECT_EQ_U(s.profiles[0].counter.count, 5);
    EXPECT_EQ_U(s.profiles[1].counter.count, 6);
    EXPECT_EQ_U(save_and_reload(&s, &loaded), DhikrLoadOk);
    EXPECT_EQ_U(loaded.profiles[1].counter.count, 6);

    // Undo at 0: nothing changes, nothing needs saving, a hint is shown.
    s.profiles[1].counter.count = 0;
    o = dhikr_counter_apply(&s, &banner, DhikrCounterActionUndo, 0);
    EXPECT(!o.changed && !o.save_now);
    EXPECT_EQ_U(banner.kind, DhikrBannerNothingToUndo);
}

/* ========================================================================= */
/* [R4-R8] Automatic restart after the target                                */
/* ========================================================================= */

static void auto_reset_at(uint32_t target) {
    DhikrState s;
    dhikr_state_init_defaults(&s);
    DhikrBanner banner = {0};
    DhikrProfile* p = dhikr_state_active(&s);
    EXPECT(dhikr_counter_set_target(&p->counter, target));
    s.profiles[1].counter.count = 4; // another profile must stay untouched

    for(uint32_t i = 1; i < target; i++) {
        DhikrActionOutcome o = dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, i);
        EXPECT_EQ_U(o.feedback_event, DhikrFeedbackEventCount);
        EXPECT_EQ_U(p->counter.count, i);
    }
    DhikrActionOutcome o = dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 5000);
    EXPECT(o.play_feedback && o.save_now);
    EXPECT_EQ_U(o.feedback_event, DhikrFeedbackEventTarget);
    EXPECT_EQ_U(p->counter.count, 0); // reset immediately, not after the banner
    EXPECT_EQ_U(p->counter.rounds, 1);
    EXPECT_EQ_U(banner.kind, DhikrBannerTarget);
    EXPECT_EQ_U(banner.value, target); // the banner shows "target/target"
    EXPECT(dhikr_banner_is_visible(&banner, 5000));
    EXPECT_EQ_U(s.profiles[1].counter.count, 4);

    // The next OK starts again at 1.
    dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 7000);
    EXPECT_EQ_U(p->counter.count, target == 1 ? 0 : 1);
}

static void test_auto_reset_33(void) { // [R4]
    auto_reset_at(33);
}

static void test_auto_reset_99(void) { // [R5]
    auto_reset_at(99);
}

static void test_auto_reset_1(void) { // [R6]
    auto_reset_at(1);
    // With target 1 every press completes a round.
    DhikrCounter c;
    dhikr_counter_init(&c, 1);
    for(int i = 0; i < 5; i++) {
        EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementRoundCompleted);
        EXPECT_EQ_U(c.count, 0);
    }
    EXPECT_EQ_U(c.rounds, 5);
    EXPECT_EQ_U(dhikr_counter_undo(&c), DhikrUndoRoundRestored);
    EXPECT_EQ_U(c.rounds, 4);
    EXPECT_EQ_U(c.count, 0);
}

static void test_target_feedback_exactly_once_per_round(void) { // [R7]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    DhikrBanner banner = {0};
    dhikr_counter_set_target(&dhikr_state_active(&s)->counter, 33);
    int target_events = 0;
    int count_events = 0;
    for(int i = 0; i < 99; i++) {
        DhikrActionOutcome o = dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 0);
        EXPECT(o.play_feedback);
        if(o.feedback_event == DhikrFeedbackEventTarget) target_events++;
        if(o.feedback_event == DhikrFeedbackEventCount) count_events++;
    }
    EXPECT_EQ_U(target_events, 3); // rounds completed at press 33, 66 and 99
    EXPECT_EQ_U(count_events, 96);
    EXPECT_EQ_U(dhikr_state_active(&s)->counter.rounds, 3);
    EXPECT_EQ_U(dhikr_state_active(&s)->counter.count, 0);
}

static void test_input_during_target_banner(void) { // [R8]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    DhikrBanner banner = {0};
    DhikrProfile* p = dhikr_state_active(&s);
    p->counter.count = 32;

    dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 1000);
    EXPECT(dhikr_banner_is_visible(&banner, 1200));

    // A press 200 ms later, while "Target reached!" is shown, counts exactly once
    // as the first count of the new round and closes the banner.
    DhikrActionOutcome o = dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 1200);
    EXPECT_EQ_U(o.feedback_event, DhikrFeedbackEventCount);
    EXPECT_EQ_U(p->counter.count, 1);
    EXPECT_EQ_U(p->counter.rounds, 1);
    EXPECT(!dhikr_banner_is_visible(&banner, 1200));

    // Ignored keys during the banner change nothing.
    dhikr_counter_apply(&s, &banner, DhikrCounterActionNone, 1300);
    EXPECT_EQ_U(p->counter.count, 1);

    // Banner timing: visible for 1 s, wrap-around safe.
    dhikr_banner_show(&banner, DhikrBannerTarget, 33, UINT32_MAX - 100);
    EXPECT(dhikr_banner_is_visible(&banner, UINT32_MAX));
    EXPECT(dhikr_banner_is_visible(&banner, 898)); // wrapped, 999 ms elapsed
    EXPECT(!dhikr_banner_is_visible(&banner, 899)); // 1000 ms elapsed
    EXPECT_EQ_U(dhikr_banner_remaining_ms(&banner, UINT32_MAX - 100), DHIKR_BANNER_TARGET_MS);
}

static void test_state_consistent_when_leaving_during_banner(void) {
    // The reset happens in the state right away, so saving while the banner
    // is still on screen stores the new round with count 0.
    fake_reset();
    DhikrState s, loaded;
    dhikr_state_init_defaults(&s);
    DhikrBanner banner = {0};
    dhikr_state_active(&s)->counter.count = 32;
    dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 0);
    EXPECT(dhikr_banner_is_visible(&banner, 10));
    EXPECT_EQ_U(save_and_reload(&s, &loaded), DhikrLoadOk);
    EXPECT_EQ_U(loaded.profiles[0].counter.count, 0);
    EXPECT_EQ_U(loaded.profiles[0].counter.rounds, 1);
}

/* ========================================================================= */
/* Reset confirmation (v1.0)                                                 */
/* ========================================================================= */

static void test_reset_confirmed(void) {
    DhikrState s;
    dhikr_state_init_defaults(&s);
    s.profiles[0].counter.count = 15;
    s.profiles[0].counter.rounds = 2;
    s.profiles[1].counter.count = 40;

    DhikrConfirm confirm;
    dhikr_confirm_begin(&confirm);
    EXPECT_EQ_U(confirm.selection, DhikrConfirmChoiceNo);
    EXPECT_EQ_U(dhikr_confirm_handle(&confirm, DhikrConfirmInputRight), DhikrConfirmPending);
    EXPECT_EQ_U(confirm.selection, DhikrConfirmChoiceYes);
    EXPECT_EQ_U(dhikr_confirm_handle(&confirm, DhikrConfirmInputOk), DhikrConfirmAccepted);

    dhikr_state_reset_active(&s);
    EXPECT_EQ_U(s.profiles[0].counter.count, 0);
    EXPECT_EQ_U(s.profiles[0].counter.rounds, 0);
    EXPECT_EQ_U(s.profiles[1].counter.count, 40); // only the active profile
    EXPECT_EQ_U(s.profiles[0].counter.target, DHIKR_TARGET_DEFAULT);
}

static void test_reset_cancelled(void) {
    DhikrConfirm confirm;
    dhikr_confirm_begin(&confirm);
    EXPECT_EQ_U(dhikr_confirm_handle(&confirm, DhikrConfirmInputOk), DhikrConfirmCancelled);

    dhikr_confirm_begin(&confirm);
    dhikr_confirm_handle(&confirm, DhikrConfirmInputRight);
    EXPECT_EQ_U(dhikr_confirm_handle(&confirm, DhikrConfirmInputBack), DhikrConfirmCancelled);

    dhikr_confirm_begin(&confirm);
    dhikr_confirm_handle(&confirm, DhikrConfirmInputRight);
    dhikr_confirm_handle(&confirm, DhikrConfirmInputLeft);
    EXPECT_EQ_U(dhikr_confirm_handle(&confirm, DhikrConfirmInputOk), DhikrConfirmCancelled);

    dhikr_confirm_begin(&confirm);
    dhikr_confirm_handle(&confirm, DhikrConfirmInputRight);
    dhikr_confirm_begin(&confirm);
    EXPECT_EQ_U(confirm.selection, DhikrConfirmChoiceNo);
}

/* ========================================================================= */
/* [R9-R15] Profiles                                                         */
/* ========================================================================= */

static void test_defaults(void) {
    DhikrState s;
    dhikr_state_init_defaults(&s);
    EXPECT_EQ_U(s.profile_count, 4);
    EXPECT(strcmp(s.profiles[0].name, "SubhanAllah") == 0);
    EXPECT(strcmp(s.profiles[1].name, "Alhamdulillah") == 0);
    EXPECT(strcmp(s.profiles[2].name, "Allahu Akbar") == 0);
    EXPECT(strcmp(s.profiles[3].name, "Custom Dhikr") == 0);
    EXPECT(strcmp(s.profiles[3].id, "custom") == 0);
    EXPECT(s.profiles[0].builtin && s.profiles[2].builtin && !s.profiles[3].builtin);
    EXPECT(dhikr_state_is_valid(&s));
}

static void test_create_custom_profile(void) { // [R9]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    uint8_t index = 0;
    EXPECT_EQ_U(dhikr_state_add_custom(&s, "  Istighfar ", 100, &index), DhikrProfileOk);
    EXPECT_EQ_U(index, 4);
    EXPECT_EQ_U(s.profile_count, 5);
    EXPECT(strcmp(s.profiles[4].name, "Istighfar") == 0);
    EXPECT(strcmp(s.profiles[4].id, "c1") == 0);
    EXPECT_EQ_U(s.profiles[4].counter.target, 100);
    EXPECT_EQ_U(s.profiles[4].counter.count, 0);
    EXPECT(!s.profiles[4].builtin);
    EXPECT_EQ_U(s.active, 0); // adding does not switch the profile
    EXPECT(dhikr_state_is_valid(&s));

    // Empty, blank, too long or invalid target are rejected.
    EXPECT_EQ_U(dhikr_state_add_custom(&s, "", 33, NULL), DhikrProfileErrorInvalidName);
    EXPECT_EQ_U(dhikr_state_add_custom(&s, "   ", 33, NULL), DhikrProfileErrorInvalidName);
    EXPECT_EQ_U(
        dhikr_state_add_custom(&s, "12345678901234567", 33, NULL), DhikrProfileErrorInvalidName);
    EXPECT_EQ_U(dhikr_state_add_custom(&s, "Valid", 0, NULL), DhikrProfileErrorInvalid);
    EXPECT_EQ_U(s.profile_count, 5);
}

static void test_manage_multiple_custom_profiles(void) { // [R10]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    uint8_t a, b, c;
    EXPECT_EQ_U(dhikr_state_add_custom(&s, "Istighfar", 100, &a), DhikrProfileOk);
    EXPECT_EQ_U(dhikr_state_add_custom(&s, "Salawat", 10, &b), DhikrProfileOk);
    EXPECT_EQ_U(dhikr_state_add_custom(&s, "Morning Dhikr", 3, &c), DhikrProfileOk);
    EXPECT(strcmp(s.profiles[b].id, "c2") == 0);
    EXPECT(strcmp(s.profiles[c].id, "c3") == 0);

    // Duplicate names (case-insensitive) are rejected.
    EXPECT_EQ_U(dhikr_state_add_custom(&s, "salawat", 33, NULL), DhikrProfileErrorDuplicateName);
    EXPECT_EQ_U(
        dhikr_state_add_custom(&s, "SUBHANALLAH", 33, NULL), DhikrProfileErrorDuplicateName);

    // Independent counters while switching.
    DhikrBanner banner = {0};
    dhikr_state_select(&s, a);
    for(int i = 0; i < 5; i++)
        dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 0);
    dhikr_state_select(&s, c);
    for(int i = 0; i < 4; i++)
        dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 0);
    dhikr_counter_apply(&s, &banner, DhikrCounterActionNextProfile, 0); // wraps to 0
    EXPECT_EQ_U(s.active, 0);
    dhikr_counter_apply(&s, &banner, DhikrCounterActionPrevProfile, 0); // back to c
    EXPECT_EQ_U(s.active, c);

    EXPECT_EQ_U(s.profiles[a].counter.count, 5);
    EXPECT_EQ_U(s.profiles[b].counter.count, 0);
    EXPECT_EQ_U(s.profiles[c].counter.count, 1); // target 3: 3 -> round, then 1
    EXPECT_EQ_U(s.profiles[c].counter.rounds, 1);
    EXPECT_EQ_U(s.profiles[0].counter.count, 0);

    char name[DHIKR_PROFILE_NAME_SIZE];
    dhikr_state_suggest_name(&s, name, sizeof(name));
    EXPECT(strcmp(name, "Dhikr 1") == 0);
    EXPECT_EQ_U(dhikr_state_add_custom(&s, name, 33, NULL), DhikrProfileOk);
    dhikr_state_suggest_name(&s, name, sizeof(name));
    EXPECT(strcmp(name, "Dhikr 2") == 0);
}

static void test_rename_custom_profile(void) { // [R11]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    uint8_t a;
    dhikr_state_add_custom(&s, "Salawat", 33, &a);
    s.profiles[a].counter.count = 9;

    EXPECT_EQ_U(dhikr_state_rename(&s, a, "  Salawat Ibrahimiya "), DhikrProfileErrorInvalidName);
    EXPECT_EQ_U(dhikr_state_rename(&s, a, " Salawat 2 "), DhikrProfileOk);
    EXPECT(strcmp(s.profiles[a].name, "Salawat 2") == 0);
    EXPECT_EQ_U(s.profiles[a].counter.count, 9);
    EXPECT(strcmp(s.profiles[a].id, "c1") == 0); // id is stable
    // Changing only the case of its own name is allowed.
    EXPECT_EQ_U(dhikr_state_rename(&s, a, "SALAWAT 2"), DhikrProfileOk);
    // Duplicate / empty / built-in.
    EXPECT_EQ_U(dhikr_state_rename(&s, a, "custom dhikr"), DhikrProfileErrorDuplicateName);
    EXPECT_EQ_U(dhikr_state_rename(&s, a, ""), DhikrProfileErrorInvalidName);
    EXPECT_EQ_U(dhikr_state_rename(&s, a, "bad\nname"), DhikrProfileErrorInvalidName);
    EXPECT_EQ_U(dhikr_state_rename(&s, 0, "Other"), DhikrProfileErrorBuiltin);
    EXPECT(strcmp(s.profiles[0].name, "SubhanAllah") == 0);
    EXPECT_EQ_U(dhikr_state_rename(&s, 3, "My Dhikr"), DhikrProfileOk); // v1.0 custom profile
    EXPECT_EQ_U(dhikr_state_rename(&s, 99, "x"), DhikrProfileErrorInvalid);
}

static void test_delete_custom_profile(void) { // [R12]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    uint8_t a, b, c;
    dhikr_state_add_custom(&s, "Istighfar", 33, &a); // 4
    dhikr_state_add_custom(&s, "Salawat", 33, &b); // 5
    dhikr_state_add_custom(&s, "Morning Dhikr", 33, &c); // 6
    s.profiles[c].counter.count = 7;

    // Deleting a profile before the active one keeps the same profile active.
    dhikr_state_select(&s, c);
    EXPECT_EQ_U(dhikr_state_delete(&s, a), DhikrProfileOk);
    EXPECT_EQ_U(s.profile_count, 6);
    EXPECT(strcmp(dhikr_state_active(&s)->name, "Morning Dhikr") == 0);
    EXPECT_EQ_U(dhikr_state_active(&s)->counter.count, 7);

    // Deleting the active profile activates the previous one.
    EXPECT_EQ_U(dhikr_state_delete(&s, s.active), DhikrProfileOk);
    EXPECT(strcmp(dhikr_state_active(&s)->name, "Salawat") == 0);

    // Deleting after the active one changes nothing for the active profile.
    dhikr_state_select(&s, 0);
    EXPECT_EQ_U(dhikr_state_delete(&s, 4), DhikrProfileOk); // Salawat
    EXPECT_EQ_U(s.active, 0);
    EXPECT_EQ_U(s.profile_count, 4);

    // The v1.0 custom profile is a normal custom profile and can be deleted too.
    EXPECT_EQ_U(dhikr_state_delete(&s, 3), DhikrProfileOk);
    EXPECT_EQ_U(s.profile_count, 3);
    EXPECT(dhikr_state_is_valid(&s));

    // Ids are never reused.
    uint8_t d;
    dhikr_state_add_custom(&s, "New", 33, &d);
    EXPECT(strcmp(s.profiles[d].id, "c4") == 0);
}

static void test_builtin_profiles_protected(void) { // [R13]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    for(uint8_t i = 0; i < DHIKR_BUILTIN_PROFILE_COUNT; i++) {
        EXPECT_EQ_U(dhikr_state_delete(&s, i), DhikrProfileErrorBuiltin);
        EXPECT_EQ_U(dhikr_state_rename(&s, i, "Renamed"), DhikrProfileErrorBuiltin);
    }
    EXPECT_EQ_U(s.profile_count, 4);
    EXPECT_EQ_U(dhikr_state_delete(&s, 200), DhikrProfileErrorInvalid);
    // Built-ins can still get their own target.
    EXPECT(dhikr_counter_set_target(&s.profiles[1].counter, 99));
}

static void test_profile_limit(void) { // [R14]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    char name[DHIKR_PROFILE_NAME_SIZE];
    while(dhikr_state_can_add(&s)) {
        dhikr_state_suggest_name(&s, name, sizeof(name));
        EXPECT_EQ_U(dhikr_state_add_custom(&s, name, 33, NULL), DhikrProfileOk);
    }
    EXPECT_EQ_U(s.profile_count, DHIKR_PROFILE_MAX);
    EXPECT_EQ_U(dhikr_state_add_custom(&s, "One more", 33, NULL), DhikrProfileErrorFull);
    EXPECT_EQ_U(s.profile_count, DHIKR_PROFILE_MAX);
    EXPECT(dhikr_state_is_valid(&s));

    // A full state still fits into the settings file and survives a reload.
    fake_reset();
    DhikrState loaded;
    EXPECT_EQ_U(save_and_reload(&s, &loaded), DhikrLoadOk);
    EXPECT(states_equal(&s, &loaded));

    // Deleting one makes room again.
    EXPECT_EQ_U(dhikr_state_delete(&s, 5), DhikrProfileOk);
    EXPECT(dhikr_state_can_add(&s));
}

static void test_active_marker_and_persistence(void) { // [R15]
    fake_reset();
    DhikrState s, loaded;
    dhikr_state_init_defaults(&s);
    uint8_t a;
    dhikr_state_add_custom(&s, "Salawat", 33, &a);
    char label[40];

    dhikr_state_profile_label(&s, 0, label, sizeof(label));
    EXPECT(strcmp(label, "* SubhanAllah") == 0);
    dhikr_state_profile_label(&s, a, label, sizeof(label));
    EXPECT(strcmp(label, "  Salawat") == 0);

    EXPECT(dhikr_state_select(&s, a));
    dhikr_state_profile_label(&s, 0, label, sizeof(label));
    EXPECT(strcmp(label, "  SubhanAllah") == 0);
    dhikr_state_profile_label(&s, a, label, sizeof(label));
    EXPECT(strcmp(label, "* Salawat") == 0);

    EXPECT_EQ_U(save_and_reload(&s, &loaded), DhikrLoadOk);
    EXPECT_EQ_U(loaded.active, a);
    EXPECT(strcmp(dhikr_state_active(&loaded)->id, "c1") == 0);
}

/* ========================================================================= */
/* [R16-R18] Storage, migration, interruption                                */
/* ========================================================================= */

/* A settings file exactly as v1.0 wrote it. */
static const char* const V1_BODY = "FlipperDhikr\n"
                                   "version=1\n"
                                   "active=custom\n"
                                   "feedback=3\n"
                                   "profile.subhanallah.count=12\n"
                                   "profile.subhanallah.target=33\n"
                                   "profile.alhamdulillah.count=99\n"
                                   "profile.alhamdulillah.target=99\n"
                                   "profile.allahu_akbar.count=39\n"
                                   "profile.allahu_akbar.target=33\n"
                                   "profile.custom.count=5\n"
                                   "profile.custom.target=1234\n"
                                   "profile.custom.name=My Dhikr\n";

static void expect_migrated_v1(const DhikrState* s) {
    EXPECT_EQ_U(s->profile_count, 4);
    DhikrFeedbackSettings all_on = fb(true, true, true, true); // v1.0 mode "Both"
    EXPECT(feedback_equal(&s->feedback, &all_on));
    EXPECT_EQ_U(s->active, 3);
    EXPECT_EQ_U(s->profiles[0].counter.count, 12);
    EXPECT_EQ_U(s->profiles[0].counter.rounds, 0);
    // v1.0 counted past the target: folded into rounds, nothing lost.
    EXPECT_EQ_U(s->profiles[1].counter.count, 0);
    EXPECT_EQ_U(s->profiles[1].counter.rounds, 1);
    EXPECT_EQ_U(s->profiles[2].counter.count, 6);
    EXPECT_EQ_U(s->profiles[2].counter.rounds, 1);
    EXPECT(strcmp(s->profiles[3].id, "custom") == 0);
    EXPECT(strcmp(s->profiles[3].name, "My Dhikr") == 0);
    EXPECT(!s->profiles[3].builtin);
    EXPECT_EQ_U(s->profiles[3].counter.count, 5);
    EXPECT_EQ_U(s->profiles[3].counter.target, 1234);
    EXPECT_EQ_U(s->next_custom_id, 1);
}

static void test_migrate_v1_file(void) { // [R16]
    char v1[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t v1_len = build_file(v1, sizeof(v1), V1_BODY);

    // Codec level.
    DhikrState s;
    uint32_t version = 0;
    EXPECT_EQ_U(dhikr_settings_parse(v1, v1_len, &s, &version), DhikrCodecOk);
    EXPECT_EQ_U(version, 1);
    expect_migrated_v1(&s);

    // Store level: backup, rewrite as v2, reload.
    fake_reset();
    fake_put(&g_fs, DHIKR_SETTINGS_PATH, v1, v1_len);
    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    DhikrState loaded;
    EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadMigrated);
    expect_migrated_v1(&loaded);
    FakeFile* backup = fake_find(&g_fs, DHIKR_SETTINGS_V1_BACKUP_PATH);
    EXPECT(backup != NULL);
    EXPECT(backup && backup->length == v1_len && memcmp(backup->data, v1, v1_len) == 0);

    // The migrated state is not "unchanged": the app's first save writes v2.
    EXPECT_EQ_U(dhikr_store_save(&store, &loaded), DhikrSaveWritten);
    FakeFile* main_file = fake_find(&g_fs, DHIKR_SETTINGS_PATH);
    EXPECT(main_file && strstr(main_file->data, "version=3\n") != NULL);

    DhikrStore store2;
    DhikrState reloaded;
    dhikr_store_init(&store2, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store2, &reloaded), DhikrLoadOk);
    EXPECT(states_equal(&loaded, &reloaded));

    // New profiles after migration get fresh ids.
    uint8_t index;
    EXPECT_EQ_U(dhikr_state_add_custom(&reloaded, "Istighfar", 33, &index), DhikrProfileOk);
    EXPECT(strcmp(reloaded.profiles[index].id, "c1") == 0);
}

static void test_migrate_v1_without_custom_name(void) {
    // v1.0 files are tolerated without the optional custom name.
    const char* body = "FlipperDhikr\nversion=1\nactive=allahu_akbar\nfeedback=0\n"
                       "profile.custom.count=3\nprofile.custom.target=33\n";
    char v1[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t len = build_file(v1, sizeof(v1), body);
    DhikrState s;
    EXPECT_EQ_U(dhikr_settings_parse(v1, len, &s, NULL), DhikrCodecOk);
    EXPECT(strcmp(s.profiles[3].name, "Custom Dhikr") == 0);
    EXPECT_EQ_U(s.profiles[3].counter.count, 3);
    EXPECT_EQ_U(s.active, 2);
    DhikrFeedbackSettings all_off = fb(false, false, false, false);
    EXPECT(feedback_equal(&s.feedback, &all_off));
}

static void test_persist_custom_profiles(void) { // [R17]
    fake_reset();
    DhikrState s, loaded;
    dhikr_state_init_defaults(&s);
    uint8_t a, b, c;
    dhikr_state_add_custom(&s, "Istighfar", 100, &a);
    dhikr_state_add_custom(&s, "Salawat", 10, &b);
    dhikr_state_add_custom(&s, "Morning Dhikr", 3, &c);
    s.profiles[a].counter.count = 55;
    s.profiles[b].counter.rounds = 4;
    s.profiles[c].counter.count = 2;
    dhikr_state_delete(&s, 3); // drop the v1.0 custom profile
    dhikr_state_select(&s, 4); // Salawat
    s.feedback = fb(false, true, true, false);

    EXPECT_EQ_U(save_and_reload(&s, &loaded), DhikrLoadOk);
    EXPECT(states_equal(&s, &loaded));
    EXPECT_EQ_U(loaded.profile_count, 6);
    EXPECT(strcmp(loaded.profiles[3].name, "Istighfar") == 0);
    EXPECT_EQ_U(loaded.profiles[3].counter.count, 55);
    EXPECT_EQ_U(loaded.profiles[3].counter.target, 100);
    EXPECT_EQ_U(loaded.profiles[4].counter.rounds, 4);
    EXPECT_EQ_U(loaded.next_custom_id, 4);
    EXPECT(strcmp(dhikr_state_active(&loaded)->name, "Salawat") == 0);
    EXPECT(dhikr_state_find(&loaded, "custom") < 0);
}

static void test_interrupted_save(void) { // [R18]
    fake_reset();
    DhikrState s, loaded;
    dhikr_state_init_defaults(&s);
    dhikr_state_add_custom(&s, "Istighfar", 100, NULL);
    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_save(&store, &s), DhikrSaveWritten);

    // a) Write of the temp file fails: old file intact.
    DhikrState changed = s;
    dhikr_state_add_custom(&changed, "Salawat", 10, NULL);
    g_fs.write_error = true;
    EXPECT_EQ_U(dhikr_store_save(&store, &changed), DhikrSaveFailed);
    g_fs.write_error = false;
    DhikrStore store2;
    dhikr_store_init(&store2, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store2, &loaded), DhikrLoadOk);
    EXPECT(states_equal(&s, &loaded));

    // b) Rename fails after the temp file was written: the complete temp file
    //    is left behind; if the main file is lost, it is recovered.
    g_fs.rename_error = true;
    EXPECT_EQ_U(dhikr_store_save(&store, &changed), DhikrSaveFailed);
    g_fs.rename_error = false;
    EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_TMP_PATH) != NULL);
    fake_remove(&g_fs, DHIKR_SETTINGS_PATH); // crash between remove and rename
    DhikrStore store3;
    dhikr_store_init(&store3, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store3, &loaded), DhikrLoadRecovered);
    EXPECT(states_equal(&changed, &loaded));

    // c) Migration interrupted (save fails): the v1 file stays and migrates again.
    fake_reset();
    char v1[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t v1_len = build_file(v1, sizeof(v1), V1_BODY);
    fake_put(&g_fs, DHIKR_SETTINGS_PATH, v1, v1_len);
    DhikrStore store4;
    dhikr_store_init(&store4, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store4, &loaded), DhikrLoadMigrated);
    g_fs.rename_error = true;
    EXPECT_EQ_U(dhikr_store_save(&store4, &loaded), DhikrSaveFailed);
    g_fs.rename_error = false;
    fake_remove(&g_fs, DHIKR_SETTINGS_TMP_PATH);
    DhikrStore store5;
    dhikr_store_init(&store5, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store5, &loaded), DhikrLoadMigrated);
    expect_migrated_v1(&loaded);

    // d) An incomplete temp file is discarded, never loaded.
    fake_reset();
    char buffer[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t length = dhikr_settings_serialize(&s, buffer, sizeof(buffer));
    fake_put(&g_fs, DHIKR_SETTINGS_TMP_PATH, buffer, length / 2);
    DhikrStore store6;
    dhikr_store_init(&store6, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store6, &loaded), DhikrLoadMissing);
    EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_TMP_PATH) == NULL);
}

static void test_codec_roundtrip(void) {
    DhikrState s, loaded;
    dhikr_state_init_defaults(&s);
    s.profiles[0].counter.count = 12;
    s.profiles[0].counter.rounds = 999999;
    s.profiles[1].counter.target = 9999;
    dhikr_state_rename(&s, 3, "Name = with eq");
    dhikr_state_add_custom(&s, "x", 1, NULL);
    s.active = 4;
    s.feedback = fb(true, false, false, true);
    char buffer[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t length = dhikr_settings_serialize(&s, buffer, sizeof(buffer));
    EXPECT(length > 0);
    uint32_t version = 0;
    EXPECT_EQ_U(dhikr_settings_parse(buffer, length, &loaded, &version), DhikrCodecOk);
    EXPECT_EQ_U(version, DHIKR_SETTINGS_VERSION);
    EXPECT(states_equal(&s, &loaded));

    char tiny[16];
    EXPECT_EQ_U(dhikr_settings_serialize(&s, tiny, sizeof(tiny)), 0);
}

static void test_store_missing_file(void) {
    fake_reset();
    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    DhikrState loaded, defaults;
    dhikr_state_init_defaults(&defaults);
    loaded.active = 77;
    EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadMissing);
    EXPECT(states_equal(&loaded, &defaults));
    EXPECT(!store.read_only);
    EXPECT_EQ_U(dhikr_store_save(&store, &loaded), DhikrSaveWritten);
}

static void test_store_corrupt_file(void) {
    static const char* bad_files[] = {
        "",
        "garbage",
        "FlipperDhikr\n",
        "FlipperDhikr\nversion=2\nactive=subhanallah\nfeedback=1\n", // no checksum
        "FlipperDhikr\nversion=0\ncrc32=00000000\n",
        "FlipperDhikr\nversion=abc\ncrc32=00000000\n",
    };
    DhikrState defaults;
    dhikr_state_init_defaults(&defaults);

    for(size_t i = 0; i < sizeof(bad_files) / sizeof(bad_files[0]); i++) {
        fake_reset();
        fake_put(&g_fs, DHIKR_SETTINGS_PATH, bad_files[i], strlen(bad_files[i]));
        DhikrStore store;
        DhikrState loaded;
        dhikr_store_init(&store, &g_io);
        EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadCorrupt);
        EXPECT(states_equal(&loaded, &defaults));
        EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_BACKUP_PATH) != NULL);
        EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_PATH) == NULL);
    }
}

static void test_store_skips_unchanged_writes(void) {
    fake_reset();
    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    DhikrState s;
    dhikr_state_init_defaults(&s);
    EXPECT_EQ_U(dhikr_store_save(&store, &s), DhikrSaveWritten);
    EXPECT_EQ_U(dhikr_store_save(&store, &s), DhikrSaveUnchanged);
    EXPECT_EQ_U(g_fs.write_count, 1);
    dhikr_counter_increment(&s.profiles[0].counter);
    EXPECT_EQ_U(dhikr_store_save(&store, &s), DhikrSaveWritten);
    EXPECT_EQ_U(g_fs.write_count, 2);
    // The undo information is not persisted and does not cause writes.
    s.profiles[0].counter.undo_round_available = true;
    EXPECT_EQ_U(dhikr_store_save(&store, &s), DhikrSaveUnchanged);

    DhikrStore store2;
    DhikrState loaded;
    dhikr_store_init(&store2, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store2, &loaded), DhikrLoadOk);
    EXPECT_EQ_U(dhikr_store_save(&store2, &loaded), DhikrSaveUnchanged);
    EXPECT_EQ_U(g_fs.write_count, 2);
}

static void test_codec_detects_tampering(void) {
    DhikrState s, loaded;
    dhikr_state_init_defaults(&s);
    s.profiles[0].counter.count = 12;
    char buffer[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t length = dhikr_settings_serialize(&s, buffer, sizeof(buffer));
    char* p = strstr(buffer, "count=12");
    EXPECT(p != NULL);
    if(p) p[6] = '9';
    EXPECT_EQ_U(dhikr_settings_parse(buffer, length, &loaded, NULL), DhikrCodecCorrupt);
    EXPECT_EQ_U(loaded.profiles[0].counter.count, 0);

    length = dhikr_settings_serialize(&s, buffer, sizeof(buffer));
    EXPECT_EQ_U(dhikr_settings_parse(buffer, length / 2, &loaded, NULL), DhikrCodecCorrupt);
}

#define V2_HEAD "FlipperDhikr\nversion=2\nactive=subhanallah\nfeedback=1\nnext_custom_id=3\n"
#define C1_OK   "profile.c1.name=A\nprofile.c1.count=0\nprofile.c1.target=33\n"

static void test_codec_rejects_invalid_values(void) {
    static const char* bodies[] = {
        // v1 values
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=9\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.subhanallah.target=0\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.subhanallah.target=10000\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.subhanallah.count=1000000\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.subhanallah.count=-1\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.custom.name=\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.custom.name=12345678901234567\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nnot a key value line\n",
        // v2 structure
        V2_HEAD "profile.c1.count=0\n", // custom_profiles missing
        V2_HEAD "custom_profiles=c1\nprofile.c1.count=0\nprofile.c1.target=33\n", // no name
        V2_HEAD "custom_profiles=c1\nprofile.c1.name=A\nprofile.c1.target=33\n", // no count
        V2_HEAD "custom_profiles=c1,c1\n" C1_OK, // duplicate id
        V2_HEAD "custom_profiles=subhanallah\n", // built-in listed as custom
        V2_HEAD "custom_profiles=x1\n", // invalid id
        V2_HEAD "custom_profiles=c01\n", // leading zero
        V2_HEAD "custom_profiles=c1,\n" C1_OK, // empty id
        V2_HEAD "custom_profiles=c1\ncustom_profiles=c1\n" C1_OK, // listed twice
        V2_HEAD "custom_profiles=c1\n" C1_OK "profile.c1.rounds=1000000\n",
        "FlipperDhikr\nversion=2\nactive=subhanallah\nfeedback=1\nnext_custom_id=0\n"
        "custom_profiles=\n",
        V2_HEAD "custom_profiles=c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14\n", // > 16
    };
    char buffer[DHIKR_SETTINGS_BUFFER_SIZE];
    DhikrState loaded;
    for(size_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); i++) {
        size_t length = build_file(buffer, sizeof(buffer), bodies[i]);
        DhikrCodecResult result = dhikr_settings_parse(buffer, length, &loaded, NULL);
        if(result != DhikrCodecCorrupt) printf("    body %zu parsed unexpectedly\n", i);
        EXPECT_EQ_U(result, DhikrCodecCorrupt);
    }
}

static void test_codec_forward_compatible(void) {
    const char* body = "FlipperDhikr\r\nversion=2\r\nactive=c7\r\nfeedback=2\r\n"
                       "next_custom_id=2\r\n"
                       "custom_profiles=c7\r\n"
                       "future.option=42\r\n"
                       "profile.removed_profile.count=5\r\n"
                       "profile.alhamdulillah.count=8\r\n"
                       "profile.alhamdulillah.color=green\r\n"
                       "profile.c7.name=Seven\r\n"
                       "profile.c7.count=1\r\n"
                       "profile.c7.target=9\r\n";
    char buffer[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t length = build_file(buffer, sizeof(buffer), body);
    DhikrState loaded;
    EXPECT_EQ_U(dhikr_settings_parse(buffer, length, &loaded, NULL), DhikrCodecOk);
    EXPECT_EQ_U(loaded.profile_count, 4);
    EXPECT_EQ_U(loaded.active, 3);
    DhikrFeedbackSettings sound_only = fb(false, true, false, true); // v1.0.1 mode "Sound"
    EXPECT(feedback_equal(&loaded.feedback, &sound_only));
    EXPECT_EQ_U(loaded.profiles[1].counter.count, 8);
    EXPECT_EQ_U(loaded.profiles[3].counter.rounds, 0); // optional
    EXPECT_EQ_U(loaded.next_custom_id, 8); // raised above the highest id in use
    EXPECT_EQ_U(loaded.profiles[0].counter.target, DHIKR_TARGET_DEFAULT);

    // All custom profiles deleted is a valid file too.
    const char* empty = "FlipperDhikr\nversion=2\nactive=zzz\nfeedback=1\nnext_custom_id=5\n"
                        "custom_profiles=\n";
    length = build_file(buffer, sizeof(buffer), empty);
    EXPECT_EQ_U(dhikr_settings_parse(buffer, length, &loaded, NULL), DhikrCodecOk);
    EXPECT_EQ_U(loaded.profile_count, 3);
    EXPECT_EQ_U(loaded.active, 0); // unknown active id
}

static void test_store_unsupported_version(void) {
    fake_reset();
    const char* future = "FlipperDhikr\nversion=4\nsomething new\n";
    fake_put(&g_fs, DHIKR_SETTINGS_PATH, future, strlen(future));
    DhikrStore store;
    DhikrState loaded;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadUnsupported);
    FakeFile* backup = fake_find(&g_fs, DHIKR_SETTINGS_BACKUP_PATH);
    EXPECT(backup != NULL);
    EXPECT(backup && backup->length == strlen(future));
}

static void test_store_read_error_protects_file(void) {
    fake_reset();
    DhikrState s, loaded;
    dhikr_state_init_defaults(&s);
    s.profiles[0].counter.count = 3;
    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_save(&store, &s), DhikrSaveWritten);

    g_fs.read_error = true;
    DhikrStore store2;
    dhikr_store_init(&store2, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store2, &loaded), DhikrLoadIoError);
    EXPECT(store2.read_only);
    EXPECT_EQ_U(dhikr_store_save(&store2, &loaded), DhikrSaveFailed);
    g_fs.read_error = false;

    DhikrStore store3;
    dhikr_store_init(&store3, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store3, &loaded), DhikrLoadOk);
    EXPECT(states_equal(&s, &loaded));
}

static void test_store_oversized_file(void) {
    fake_reset();
    static char big[FAKE_FILE_SIZE];
    memset(big, 'A', sizeof(big));
    fake_put(&g_fs, DHIKR_SETTINGS_PATH, big, sizeof(big));
    DhikrStore store;
    DhikrState loaded;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadCorrupt);
}

/* ========================================================================= */
/* v1.0.2: independent feedback switches  [F<n>] = v1.0.2 test list item n   */
/* ========================================================================= */

/* Patterns an action produces, exactly as the counter scene plays them. */
typedef struct {
    int count_patterns;
    int target_patterns;
    int none;
} PatternTally;

static void tally(PatternTally* t, const DhikrFeedbackSettings* settings, DhikrActionOutcome o) {
    if(!o.play_feedback) return;
    DhikrFeedbackPattern p = dhikr_feedback_select(settings, o.feedback_event);
    if(p == DhikrFeedbackPatternNone) {
        t->none++;
    } else if(dhikr_feedback_pattern_is_target(p)) {
        t->target_patterns++;
    } else {
        t->count_patterns++;
    }
}

static void expect_patterns(
    DhikrFeedbackSettings settings,
    DhikrFeedbackPattern count,
    DhikrFeedbackPattern target) {
    EXPECT_EQ_U(dhikr_feedback_select(&settings, DhikrFeedbackEventCount), count);
    EXPECT_EQ_U(dhikr_feedback_select(&settings, DhikrFeedbackEventTarget), target);
}

static void test_feedback_defaults(void) {
    DhikrState s;
    dhikr_state_init_defaults(&s);
    DhikrFeedbackSettings expected = fb(false, false, true, true);
    EXPECT(feedback_equal(&s.feedback, &expected));
    // Fresh install (no file) uses the same defaults.
    fake_reset();
    DhikrStore store;
    DhikrState loaded;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadMissing);
    EXPECT(feedback_equal(&loaded.feedback, &expected));
}

static void test_feedback_each_switch(void) { // [F1]
    // Each switch on its own, then off again.
    expect_patterns(fb(true, false, false, false), DhikrFeedbackPatternCountVibro, DhikrFeedbackPatternNone);
    expect_patterns(fb(false, true, false, false), DhikrFeedbackPatternCountSound, DhikrFeedbackPatternNone);
    expect_patterns(fb(false, false, true, false), DhikrFeedbackPatternNone, DhikrFeedbackPatternTargetVibro);
    expect_patterns(fb(false, false, false, true), DhikrFeedbackPatternNone, DhikrFeedbackPatternTargetSound);
    DhikrFeedbackSettings s = fb(true, true, true, true);
    s.counter_vibration = false;
    expect_patterns(s, DhikrFeedbackPatternCountSound, DhikrFeedbackPatternTargetBoth);
    s.counter_sound = false;
    expect_patterns(s, DhikrFeedbackPatternNone, DhikrFeedbackPatternTargetBoth);
    s.target_vibration = false;
    expect_patterns(s, DhikrFeedbackPatternNone, DhikrFeedbackPatternTargetSound);
    s.target_sound = false;
    expect_patterns(s, DhikrFeedbackPatternNone, DhikrFeedbackPatternNone);
    s.target_vibration = true;
    expect_patterns(s, DhikrFeedbackPatternNone, DhikrFeedbackPatternTargetVibro);
}

static void test_feedback_named_combinations(void) { // [F2-F9]
    // F2: counter off, target on (the new default): silent counting, signal at the target.
    expect_patterns(fb(false, false, true, true), DhikrFeedbackPatternNone, DhikrFeedbackPatternTargetBoth);
    // F3: counter on, target off.
    expect_patterns(fb(true, true, false, false), DhikrFeedbackPatternCountBoth, DhikrFeedbackPatternNone);
    // F4-F7: exactly one switch.
    expect_patterns(fb(true, false, false, false), DhikrFeedbackPatternCountVibro, DhikrFeedbackPatternNone);
    expect_patterns(fb(false, true, false, false), DhikrFeedbackPatternCountSound, DhikrFeedbackPatternNone);
    expect_patterns(fb(false, false, true, false), DhikrFeedbackPatternNone, DhikrFeedbackPatternTargetVibro);
    expect_patterns(fb(false, false, false, true), DhikrFeedbackPatternNone, DhikrFeedbackPatternTargetSound);
    // F8: everything on.
    expect_patterns(fb(true, true, true, true), DhikrFeedbackPatternCountBoth, DhikrFeedbackPatternTargetBoth);
    // F9: everything off.
    expect_patterns(fb(false, false, false, false), DhikrFeedbackPatternNone, DhikrFeedbackPatternNone);
    DhikrFeedbackSettings off = fb(false, false, false, false);
    EXPECT(!dhikr_feedback_any_enabled(&off));
    EXPECT_EQ_U(dhikr_feedback_select(NULL, DhikrFeedbackEventTarget), DhikrFeedbackPatternNone);
}

static void test_feedback_all_16_combinations(void) {
    fake_reset();
    for(unsigned mask = 0; mask < 16; mask++) {
        const bool cv = mask & 1, cs = mask & 2, tv = mask & 4, ts = mask & 8;
        DhikrFeedbackSettings settings = fb(cv, cs, tv, ts);

        DhikrFeedbackPattern count = dhikr_feedback_select(&settings, DhikrFeedbackEventCount);
        DhikrFeedbackPattern target = dhikr_feedback_select(&settings, DhikrFeedbackEventTarget);
        // A count only uses the counter switches, a target only the target switches.
        EXPECT_EQ_U(dhikr_feedback_pattern_uses_vibro(count), cv);
        EXPECT_EQ_U(dhikr_feedback_pattern_uses_sound(count), cs);
        EXPECT(!dhikr_feedback_pattern_is_target(count));
        EXPECT_EQ_U(dhikr_feedback_pattern_uses_vibro(target), tv);
        EXPECT_EQ_U(dhikr_feedback_pattern_uses_sound(target), ts);
        EXPECT_EQ_U(target != DhikrFeedbackPatternNone, tv || ts);
        EXPECT(target == DhikrFeedbackPatternNone || dhikr_feedback_pattern_is_target(target));
        EXPECT_EQ_U(dhikr_feedback_any_enabled(&settings), mask != 0);

        // One full round with target 3: two counts and one completion.
        DhikrState s;
        dhikr_state_init_defaults(&s);
        s.feedback = settings;
        dhikr_counter_set_target(&dhikr_state_active(&s)->counter, 3);
        DhikrBanner banner = {0};
        PatternTally t = {0};
        for(int i = 0; i < 3; i++) {
            tally(&t, &s.feedback, dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 0));
        }
        EXPECT_EQ_U(t.count_patterns, (cv || cs) ? 2 : 0);
        EXPECT_EQ_U(t.target_patterns, (tv || ts) ? 1 : 0);
        EXPECT_EQ_U(dhikr_state_active(&s)->counter.rounds, 1); // auto reset unaffected
        EXPECT_EQ_U(dhikr_state_active(&s)->counter.count, 0);

        // [F11] every combination survives save + reload.
        DhikrState loaded;
        EXPECT_EQ_U(save_and_reload(&s, &loaded), DhikrLoadOk);
        EXPECT(feedback_equal(&loaded.feedback, &settings));
        EXPECT(states_equal(&s, &loaded));
    }
}

static void test_feedback_no_double_on_target(void) { // [F10]
    // Counter all on, target all off: the completing press must stay silent,
    // it must not fall back to the counter pattern.
    DhikrState s;
    dhikr_state_init_defaults(&s);
    s.feedback = fb(true, true, false, false);
    dhikr_state_active(&s)->counter.count = 32;
    DhikrBanner banner = {0};
    DhikrActionOutcome o = dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 0);
    EXPECT_EQ_U(o.feedback_event, DhikrFeedbackEventTarget);
    EXPECT_EQ_U(dhikr_feedback_select(&s.feedback, o.feedback_event), DhikrFeedbackPatternNone);
    EXPECT_EQ_U(banner.kind, DhikrBannerTarget); // visual message still shown

    // Everything on: one action -> exactly one pattern, the target one.
    s.feedback = fb(true, true, true, true);
    dhikr_state_active(&s)->counter.count = 32;
    PatternTally t = {0};
    tally(&t, &s.feedback, dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 0));
    EXPECT_EQ_U(t.target_patterns, 1);
    EXPECT_EQ_U(t.count_patterns, 0);

    // Undo never plays feedback.
    o = dhikr_counter_apply(&s, &banner, DhikrCounterActionUndo, 0);
    EXPECT(!o.play_feedback);
}

static const char* const V2_BODY_FMT = "FlipperDhikr\n"
                                       "version=2\n"
                                       "active=c1\n"
                                       "feedback=%u\n"
                                       "next_custom_id=2\n"
                                       "custom_profiles=custom,c1\n"
                                       "profile.subhanallah.count=5\n"
                                       "profile.subhanallah.target=33\n"
                                       "profile.subhanallah.rounds=7\n"
                                       "profile.custom.name=Custom Dhikr\n"
                                       "profile.custom.count=0\n"
                                       "profile.custom.target=33\n"
                                       "profile.custom.rounds=0\n"
                                       "profile.c1.name=Istighfar\n"
                                       "profile.c1.count=42\n"
                                       "profile.c1.target=100\n"
                                       "profile.c1.rounds=3\n";

static void test_feedback_migration_from_v2(void) { // [F12]
    const DhikrFeedbackSettings expected[4] = {
        fb(false, false, false, false), // Off
        fb(true, false, true, false), // Vibration
        fb(false, true, false, true), // Sound
        fb(true, true, true, true), // Both
    };
    for(unsigned mode = 0; mode < 4; mode++) {
        char body[1024];
        snprintf(body, sizeof(body), V2_BODY_FMT, mode);
        char file[DHIKR_SETTINGS_BUFFER_SIZE];
        size_t len = build_file(file, sizeof(file), body);

        fake_reset();
        fake_put(&g_fs, DHIKR_SETTINGS_PATH, file, len);
        DhikrStore store;
        DhikrState loaded;
        dhikr_store_init(&store, &g_io);
        EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadMigrated);
        EXPECT(feedback_equal(&loaded.feedback, &expected[mode]));

        // Profiles, counts, rounds and targets are untouched.
        EXPECT_EQ_U(loaded.profile_count, 5);
        EXPECT(strcmp(dhikr_state_active(&loaded)->name, "Istighfar") == 0);
        EXPECT_EQ_U(loaded.profiles[0].counter.count, 5);
        EXPECT_EQ_U(loaded.profiles[0].counter.rounds, 7);
        EXPECT_EQ_U(loaded.profiles[4].counter.count, 42);
        EXPECT_EQ_U(loaded.profiles[4].counter.target, 100);
        EXPECT_EQ_U(loaded.profiles[4].counter.rounds, 3);
        EXPECT_EQ_U(loaded.next_custom_id, 2);

        // Original kept as settings.v2.bak, new file written as v3.
        FakeFile* backup = fake_find(&g_fs, DHIKR_SETTINGS_V2_BACKUP_PATH);
        EXPECT(backup && backup->length == len && memcmp(backup->data, file, len) == 0);
        EXPECT_EQ_U(dhikr_store_save(&store, &loaded), DhikrSaveWritten);
        FakeFile* main_file = fake_find(&g_fs, DHIKR_SETTINGS_PATH);
        EXPECT(main_file && strstr(main_file->data, "version=3\n") != NULL);
        EXPECT(main_file && strstr(main_file->data, "\nfeedback=") == NULL);

        DhikrStore store2;
        DhikrState reloaded;
        dhikr_store_init(&store2, &g_io);
        EXPECT_EQ_U(dhikr_store_load(&store2, &reloaded), DhikrLoadOk);
        EXPECT(states_equal(&loaded, &reloaded));
    }

    // Legacy mapping table on its own.
    DhikrFeedbackSettings s;
    for(unsigned mode = 0; mode < 4; mode++) {
        dhikr_feedback_settings_from_legacy(&s, (DhikrFeedbackLegacyMode)mode);
        EXPECT(feedback_equal(&s, &expected[mode]));
    }
    EXPECT(!dhikr_feedback_legacy_mode_is_valid(4));
}

static void test_feedback_migration_chain_from_v1(void) { // [F12]
    // v1.0 -> current: feedback mode "Vibration" becomes both vibration switches.
    const char* body = "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\n"
                       "profile.subhanallah.count=40\nprofile.subhanallah.target=33\n";
    char file[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t len = build_file(file, sizeof(file), body);
    fake_reset();
    fake_put(&g_fs, DHIKR_SETTINGS_PATH, file, len);
    DhikrStore store;
    DhikrState loaded;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadMigrated);
    DhikrFeedbackSettings vibro = fb(true, false, true, false);
    EXPECT(feedback_equal(&loaded.feedback, &vibro));
    EXPECT_EQ_U(loaded.profiles[0].counter.count, 7);
    EXPECT_EQ_U(loaded.profiles[0].counter.rounds, 1);
    EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_V1_BACKUP_PATH) != NULL);
}

#define V3_HEAD "FlipperDhikr\nversion=3\nactive=subhanallah\nnext_custom_id=1\ncustom_profiles=\n"

static void test_feedback_corrupt_files(void) { // [F13]
    static const char* bodies[] = {
        // a switch is missing
        V3_HEAD "feedback.counter_vibration=0\nfeedback.counter_sound=0\nfeedback.target_vibration=1\n",
        // invalid values
        V3_HEAD "feedback.counter_vibration=2\nfeedback.counter_sound=0\n"
                "feedback.target_vibration=1\nfeedback.target_sound=1\n",
        V3_HEAD "feedback.counter_vibration=on\nfeedback.counter_sound=0\n"
                "feedback.target_vibration=1\nfeedback.target_sound=1\n",
        V3_HEAD "feedback.counter_vibration=\nfeedback.counter_sound=0\n"
                "feedback.target_vibration=1\nfeedback.target_sound=1\n",
        V3_HEAD "feedback.counter_vibration=01\nfeedback.counter_sound=0\n"
                "feedback.target_vibration=1\nfeedback.target_sound=1\n",
        // only the old key in a v3 file
        V3_HEAD "feedback=3\n",
        // new keys in a v2 file (no "feedback=" mode)
        "FlipperDhikr\nversion=2\nactive=subhanallah\nnext_custom_id=1\ncustom_profiles=\n"
        "feedback.counter_vibration=0\nfeedback.counter_sound=0\n"
        "feedback.target_vibration=1\nfeedback.target_sound=1\n",
    };
    char buffer[DHIKR_SETTINGS_BUFFER_SIZE];
    DhikrState loaded, defaults;
    dhikr_state_init_defaults(&defaults);
    for(size_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); i++) {
        size_t length = build_file(buffer, sizeof(buffer), bodies[i]);
        DhikrCodecResult result = dhikr_settings_parse(buffer, length, &loaded, NULL);
        if(result != DhikrCodecCorrupt) printf("    feedback body %zu parsed unexpectedly\n", i);
        EXPECT_EQ_U(result, DhikrCodecCorrupt);
        EXPECT(states_equal(&loaded, &defaults));
    }

    // The same through the store: backup + defaults, no crash.
    fake_reset();
    size_t length = build_file(buffer, sizeof(buffer), bodies[0]);
    fake_put(&g_fs, DHIKR_SETTINGS_PATH, buffer, length);
    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadCorrupt);
    EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_BACKUP_PATH) != NULL);

    // A valid v3 file with all four keys in any order parses.
    const char* ok = V3_HEAD "feedback.target_sound=0\nfeedback.counter_sound=1\n"
                             "feedback.target_vibration=1\nfeedback.counter_vibration=0\n";
    length = build_file(buffer, sizeof(buffer), ok);
    EXPECT_EQ_U(dhikr_settings_parse(buffer, length, &loaded, NULL), DhikrCodecOk);
    DhikrFeedbackSettings expected = fb(false, true, true, false);
    EXPECT(feedback_equal(&loaded.feedback, &expected));
}

/* ========================================================================= */
/* v1.0.3 menu and UX regression tests [U1-U12]                              */
/* ========================================================================= */

static void test_main_menu_without_target_settings(void) { // [U1]
    const char* expected[] = {"Start Counting", "Dhikr Profiles", "Feedback Settings", "About"};
    EXPECT_EQ_U(DhikrMainMenuCount, 4);
    for(int i = 0; i < DhikrMainMenuCount; i++) {
        const char* label = dhikr_main_menu_label((DhikrMainMenuItem)i);
        EXPECT(strcmp(label, expected[i]) == 0);
        EXPECT(strstr(label, "Target") == NULL);
        EXPECT(strstr(label, "Reset") == NULL);
    }
    EXPECT(strcmp(dhikr_main_menu_label(DhikrMainMenuCount), "") == 0);
}

static void test_edit_target_reachable_for_every_profile(void) { // [U2]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    uint8_t a;
    EXPECT_EQ_U(dhikr_state_add_custom(&s, "Istighfar", 100, &a), DhikrProfileOk);
    for(uint8_t i = 0; i < s.profile_count; i++) {
        DhikrProfileAction actions[DhikrProfileActionCount];
        uint8_t n = dhikr_profile_actions(&s.profiles[i], actions);
        EXPECT(n >= 3);
        EXPECT_EQ_U(actions[0], DhikrProfileActionProgress); // always first
        EXPECT_EQ_U(actions[1], DhikrProfileActionTarget);
        EXPECT(dhikr_profile_action_allowed(&s.profiles[i], DhikrProfileActionTarget));
    }
    char label[40];
    dhikr_profile_action_label(&s.profiles[0], DhikrProfileActionTarget, label, sizeof(label));
    EXPECT(strcmp(label, "Edit Target (33)") == 0);
    dhikr_profile_action_label(&s.profiles[a], DhikrProfileActionTarget, label, sizeof(label));
    EXPECT(strcmp(label, "Edit Target (100)") == 0);
}

static void test_edit_inactive_profile_keeps_active(void) { // [U3]
    DhikrState s, before;
    dhikr_state_init_defaults(&s);
    dhikr_state_select(&s, 1);
    s.profiles[1].counter.count = 7;
    s.profiles[1].counter.rounds = 2;
    s.profiles[2].counter.count = 12;
    s.profiles[2].counter.rounds = 4;
    before = s;

    // Edit Target on profile 2 (not active) -> 99.
    EXPECT(dhikr_state_set_target(&s, 2, 99));
    EXPECT_EQ_U(s.active, 1);
    EXPECT_EQ_U(s.profiles[2].counter.target, 99);
    EXPECT_EQ_U(s.profiles[2].counter.count, 12);
    EXPECT_EQ_U(s.profiles[2].counter.rounds, 4);
    // All other profiles are untouched.
    for(uint8_t i = 0; i < s.profile_count; i++) {
        if(i == 2) continue;
        EXPECT(memcmp(&s.profiles[i], &before.profiles[i], sizeof(DhikrProfile)) == 0);
    }
    // The active profile's marker is still on profile 1.
    char label[40];
    dhikr_state_profile_label(&s, 1, label, sizeof(label));
    EXPECT(strcmp(label, "* Alhamdulillah") == 0);
    dhikr_state_profile_label(&s, 2, label, sizeof(label));
    EXPECT(strcmp(label, "  Allahu Akbar") == 0);
}

static void test_set_target_rejects_invalid(void) { // [U4]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    EXPECT(!dhikr_state_set_target(&s, 0, 0));
    EXPECT(!dhikr_state_set_target(&s, 0, DHIKR_TARGET_MAX + 1));
    EXPECT(!dhikr_state_set_target(&s, 0, 33)); // unchanged value: nothing to save
    EXPECT(!dhikr_state_set_target(&s, s.profile_count, 99)); // no such profile
    EXPECT(!dhikr_state_set_target(NULL, 0, 99));
    EXPECT_EQ_U(s.profiles[0].counter.target, 33);
    EXPECT(dhikr_state_set_target(&s, 0, DHIKR_TARGET_MAX));
    EXPECT(dhikr_state_set_target(&s, 0, DHIKR_TARGET_MIN));
}

static void test_profile_selection_and_marker(void) { // [U5]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    uint8_t a;
    dhikr_state_add_custom(&s, "Dhikr 1", 33, &a);
    char label[40];
    for(uint8_t sel = 0; sel < s.profile_count; sel++) {
        EXPECT(dhikr_state_select(&s, sel));
        for(uint8_t i = 0; i < s.profile_count; i++) {
            dhikr_state_profile_label(&s, i, label, sizeof(label));
            EXPECT((label[0] == '*') == (i == sel)); // exactly one marker
        }
        // Editing another profile's target leaves the marker where it is.
        uint8_t other = (uint8_t)((sel + 1) % s.profile_count);
        dhikr_state_set_target(&s, other, s.profiles[other].counter.target == 99 ? 33 : 99);
        EXPECT_EQ_U(s.active, sel);
    }
}

static void test_builtin_actions_restricted(void) { // [U6]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    for(uint8_t i = 0; i < s.profile_count; i++) {
        if(!s.profiles[i].builtin) continue;
        DhikrProfileAction actions[DhikrProfileActionCount];
        EXPECT_EQ_U(dhikr_profile_actions(&s.profiles[i], actions), 3);
        EXPECT_EQ_U(actions[0], DhikrProfileActionProgress);
        EXPECT_EQ_U(actions[1], DhikrProfileActionTarget);
        EXPECT_EQ_U(actions[2], DhikrProfileActionReset);
        EXPECT(!dhikr_profile_action_allowed(&s.profiles[i], DhikrProfileActionRename));
        EXPECT(!dhikr_profile_action_allowed(&s.profiles[i], DhikrProfileActionDelete));
        // The model refuses as well, independent of the menu.
        EXPECT(dhikr_state_rename(&s, i, "X") != DhikrProfileOk);
        EXPECT(dhikr_state_delete(&s, i) != DhikrProfileOk);
        // But the target can be changed.
        EXPECT(dhikr_state_set_target(&s, i, 99));
    }
    EXPECT(!dhikr_profile_action_allowed(NULL, DhikrProfileActionTarget));
    EXPECT(!dhikr_profile_action_allowed(&s.profiles[0], DhikrProfileActionCount));
}

static void test_custom_profile_actions(void) { // [U7]
    fake_reset();
    DhikrState s, loaded;
    dhikr_state_init_defaults(&s);
    uint8_t a;
    EXPECT_EQ_U(dhikr_state_add_custom(&s, "Salawat", 33, &a), DhikrProfileOk);
    DhikrProfileAction actions[DhikrProfileActionCount];
    EXPECT_EQ_U(dhikr_profile_actions(&s.profiles[a], actions), 5);
    EXPECT_EQ_U(actions[0], DhikrProfileActionProgress);
    EXPECT_EQ_U(actions[1], DhikrProfileActionTarget);
    EXPECT_EQ_U(actions[2], DhikrProfileActionRename);
    EXPECT_EQ_U(actions[3], DhikrProfileActionReset);
    EXPECT_EQ_U(actions[4], DhikrProfileActionDelete);
    char label[40];
    dhikr_profile_action_label(&s.profiles[a], DhikrProfileActionRename, label, sizeof(label));
    EXPECT(strcmp(label, "Rename Profile") == 0);
    dhikr_profile_action_label(&s.profiles[a], DhikrProfileActionDelete, label, sizeof(label));
    EXPECT(strcmp(label, "Delete Profile") == 0);

    // Target, rename and delete work as in v1.0.2 and are saved.
    s.profiles[a].counter.count = 9;
    EXPECT(dhikr_state_set_target(&s, a, 100));
    EXPECT_EQ_U(dhikr_state_rename(&s, a, "Salawat Nabi"), DhikrProfileOk);
    EXPECT_EQ_U(save_and_reload(&s, &loaded), DhikrLoadOk);
    EXPECT(strcmp(loaded.profiles[a].name, "Salawat Nabi") == 0);
    EXPECT_EQ_U(loaded.profiles[a].counter.target, 100);
    EXPECT_EQ_U(loaded.profiles[a].counter.count, 9);
    EXPECT_EQ_U(dhikr_state_delete(&loaded, a), DhikrProfileOk);
    EXPECT_EQ_U(loaded.profile_count, 4);
}

static void test_sets_persist_after_restart(void) { // [U8]
    fake_reset();
    DhikrState s, loaded;
    dhikr_state_init_defaults(&s);
    DhikrBanner banner = {0};
    // Two full sets of 33 plus 5 counts.
    for(int i = 0; i < 2 * 33 + 5; i++) {
        dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 0);
    }
    EXPECT_EQ_U(s.profiles[0].counter.rounds, 2);
    EXPECT_EQ_U(s.profiles[0].counter.count, 5);
    EXPECT_EQ_U(save_and_reload(&s, &loaded), DhikrLoadOk);
    EXPECT_EQ_U(loaded.profiles[0].counter.rounds, 2);
    EXPECT_EQ_U(loaded.profiles[0].counter.count, 5);
    EXPECT(states_equal(&s, &loaded));
}

static void test_set_banner_and_undo(void) { // [U9]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    DhikrBanner banner = {0};
    DhikrProfile* p = dhikr_state_active(&s);
    p->counter.count = 32;

    // Completing the first set: banner "33/33 Target reached!", target feedback once.
    DhikrActionOutcome o = dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 100);
    EXPECT_EQ_U(o.feedback_event, DhikrFeedbackEventTarget);
    EXPECT_EQ_U(banner.kind, DhikrBannerTarget);
    EXPECT_EQ_U(banner.value, 33);
    char text[32];
    dhikr_counter_status_text(&p->counter, &banner, 150, text, sizeof(text));
    EXPECT(strcmp(text, "Target reached!") == 0);
    EXPECT_EQ_U(p->counter.count, 0);
    EXPECT_EQ_U(p->counter.rounds, 1);

    // Undo right after it restores the set: 32/33, Sets: 0.
    o = dhikr_counter_apply(&s, &banner, DhikrCounterActionUndo, 200);
    EXPECT(o.changed && o.save_now && !o.play_feedback);
    EXPECT_EQ_U(banner.kind, DhikrBannerRoundRestored);
    dhikr_counter_status_text(&p->counter, &banner, 250, text, sizeof(text));
    EXPECT(strcmp(text, "Set undone") == 0);
    EXPECT_EQ_U(p->counter.count, 32);
    EXPECT_EQ_U(p->counter.rounds, 0);

    // Completing it again counts exactly one set, then the next one is set 2.
    dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 300);
    EXPECT_EQ_U(banner.kind, DhikrBannerTarget);
    EXPECT_EQ_U(p->counter.rounds, 1);
    for(int i = 0; i < 32; i++) dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 400);
    EXPECT_EQ_U(banner.kind, DhikrBannerNone); // normal counts close the banner
    dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 500);
    EXPECT_EQ_U(banner.kind, DhikrBannerTarget);
    EXPECT_EQ_U(p->counter.rounds, 2);

    // Switching profiles clears the banner.
    dhikr_counter_apply(&s, &banner, DhikrCounterActionNextProfile, 600);
    EXPECT_EQ_U(banner.kind, DhikrBannerNone);
}

static void test_target_change_keeps_counts_and_sets(void) { // [U10]
    DhikrState s;
    dhikr_state_init_defaults(&s);
    DhikrBanner banner = {0};
    DhikrProfile* p = dhikr_state_active(&s);
    p->counter.count = 10;
    p->counter.rounds = 3;

    // Raising the target: count and completed sets stay as they are; the old
    // sets are not re-interpreted against the new target.
    EXPECT(dhikr_state_set_target(&s, 0, 99));
    EXPECT_EQ_U(p->counter.count, 10);
    EXPECT_EQ_U(p->counter.rounds, 3);

    // Lowering it below the count: nothing changes until the next press,
    // which completes exactly one set.
    EXPECT(dhikr_state_set_target(&s, 0, 5));
    EXPECT_EQ_U(p->counter.count, 10);
    EXPECT_EQ_U(p->counter.rounds, 3);
    dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 0);
    EXPECT_EQ_U(p->counter.count, 0);
    EXPECT_EQ_U(p->counter.rounds, 4);
    EXPECT_EQ_U(banner.kind, DhikrBannerTarget);

    // A target change right after a set clears the set undo (the restored
    // count would refer to the old target).
    EXPECT(dhikr_state_set_target(&s, 0, 33));
    EXPECT_EQ_U(dhikr_counter_undo(&p->counter), DhikrUndoNothing);
    EXPECT_EQ_U(p->counter.rounds, 4);
}

/* A settings file exactly as v1.0.2 wrote it (generated with the v1.0.2 tag). */
static const char* const V102_FILE = "FlipperDhikr\n"
                                     "version=3\n"
                                     "active=c1\n"
                                     "feedback.counter_vibration=1\n"
                                     "feedback.counter_sound=0\n"
                                     "feedback.target_vibration=1\n"
                                     "feedback.target_sound=0\n"
                                     "next_custom_id=2\n"
                                     "custom_profiles=custom,c1\n"
                                     "profile.subhanallah.count=5\n"
                                     "profile.subhanallah.target=33\n"
                                     "profile.subhanallah.rounds=7\n"
                                     "profile.alhamdulillah.count=0\n"
                                     "profile.alhamdulillah.target=99\n"
                                     "profile.alhamdulillah.rounds=2\n"
                                     "profile.allahu_akbar.count=0\n"
                                     "profile.allahu_akbar.target=33\n"
                                     "profile.allahu_akbar.rounds=0\n"
                                     "profile.custom.name=Custom Dhikr\n"
                                     "profile.custom.count=0\n"
                                     "profile.custom.target=33\n"
                                     "profile.custom.rounds=0\n"
                                     "profile.c1.name=Istighfar\n"
                                     "profile.c1.count=42\n"
                                     "profile.c1.target=100\n"
                                     "profile.c1.rounds=3\n"
                                     "crc32=2FD49427\n";

static void test_v1_0_2_settings_compatible(void) { // [U11]
    fake_reset();
    const size_t len = strlen(V102_FILE);
    fake_put(&g_fs, DHIKR_SETTINGS_PATH, V102_FILE, len);
    DhikrStore store;
    DhikrState loaded;
    dhikr_store_init(&store, &g_io);
    // Loaded as the current format: no migration, no backup.
    EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadOk);
    EXPECT_EQ_U(DHIKR_SETTINGS_VERSION, 3);
    EXPECT(strcmp(dhikr_state_active(&loaded)->name, "Istighfar") == 0);
    EXPECT_EQ_U(loaded.profiles[0].counter.rounds, 7); // sets kept
    EXPECT_EQ_U(loaded.profiles[1].counter.target, 99);
    EXPECT_EQ_U(loaded.profiles[4].counter.count, 42);
    DhikrFeedbackSettings expected = fb(true, false, true, false);
    EXPECT(feedback_equal(&loaded.feedback, &expected));

    // Writing it back produces byte-identical content: the format is unchanged.
    char buffer[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t n = dhikr_settings_serialize(&loaded, buffer, sizeof(buffer));
    EXPECT_EQ_U(n, len);
    EXPECT(n == len && memcmp(buffer, V102_FILE, len) == 0);
    EXPECT_EQ_U(dhikr_store_save(&store, &loaded), DhikrSaveUnchanged);
}

static void test_feedback_unchanged_by_ux_update(void) { // [U12]
    // All 16 switch combinations still select exactly one pattern per event, and
    // completing a set (the renamed round) still requests only the target pattern.
    for(unsigned bits = 0; bits < 16; bits++) {
        DhikrFeedbackSettings f = fb(bits & 1, bits & 2, bits & 4, bits & 8);
        DhikrFeedbackPattern count = dhikr_feedback_select(&f, DhikrFeedbackEventCount);
        DhikrFeedbackPattern target = dhikr_feedback_select(&f, DhikrFeedbackEventTarget);
        EXPECT(!dhikr_feedback_pattern_is_target(count));
        EXPECT(target == DhikrFeedbackPatternNone || dhikr_feedback_pattern_is_target(target));
        EXPECT_EQ_U(dhikr_feedback_pattern_uses_vibro(count), (bits & 1) != 0);
        EXPECT_EQ_U(dhikr_feedback_pattern_uses_sound(count), (bits & 2) != 0);
        EXPECT_EQ_U(dhikr_feedback_pattern_uses_vibro(target), (bits & 4) != 0);
        EXPECT_EQ_U(dhikr_feedback_pattern_uses_sound(target), (bits & 8) != 0);
    }
    DhikrState s;
    dhikr_state_init_defaults(&s);
    DhikrBanner banner = {0};
    dhikr_state_set_target(&s, 0, 3);
    int target_events = 0;
    for(int i = 0; i < 9; i++) {
        DhikrActionOutcome o = dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 0);
        if(o.feedback_event == DhikrFeedbackEventTarget) target_events++;
    }
    EXPECT_EQ_U(target_events, 3);
    EXPECT_EQ_U(s.profiles[0].counter.rounds, 3);
}

/* ========================================================================= */
/* v1.0.3 profile details, View Progress, per-profile reset [P1-P14]         */
/* Numbers in brackets refer to the v1.0.3 refinement test list (1-31).      */
/* ========================================================================= */

/* SubhanAllah 20/33 with 2 sets (active), Istighfar 15/100 with 4 sets. */
static uint8_t setup_reset_example(DhikrState* s) {
    dhikr_state_init_defaults(s);
    uint8_t a = 0;
    EXPECT_EQ_U(dhikr_state_add_custom(s, "Istighfar", 100, &a), DhikrProfileOk);
    s->profiles[0].counter.count = 20;
    s->profiles[0].counter.rounds = 2;
    s->profiles[a].counter.count = 15;
    s->profiles[a].counter.rounds = 4;
    s->profiles[1].counter.count = 7;
    s->profiles[1].counter.rounds = 1;
    EXPECT(dhikr_state_select(s, 0));
    return a;
}

static void test_main_menu_entries_unique(void) { // [P1] (1-3)
    for(int i = 0; i < DhikrMainMenuCount; i++) {
        const char* a = dhikr_main_menu_label((DhikrMainMenuItem)i);
        EXPECT(a[0] != '\0');
        EXPECT(strstr(a, "Reset") == NULL && strstr(a, "Target") == NULL);
        for(int j = i + 1; j < DhikrMainMenuCount; j++) {
            EXPECT(strcmp(a, dhikr_main_menu_label((DhikrMainMenuItem)j)) != 0);
        }
    }
}

static void test_profile_details_actions(void) { // [P2] (4-8)
    DhikrState s;
    setup_reset_example(&s);
    char label[40];
    for(uint8_t i = 0; i < s.profile_count; i++) {
        const DhikrProfile* p = &s.profiles[i];
        EXPECT(dhikr_profile_action_allowed(p, DhikrProfileActionProgress));
        EXPECT(dhikr_profile_action_allowed(p, DhikrProfileActionTarget));
        EXPECT(dhikr_profile_action_allowed(p, DhikrProfileActionReset));
        EXPECT_EQ_U(dhikr_profile_action_allowed(p, DhikrProfileActionRename), !p->builtin);
        EXPECT_EQ_U(dhikr_profile_action_allowed(p, DhikrProfileActionDelete), !p->builtin);
    }
    dhikr_profile_action_label(&s.profiles[0], DhikrProfileActionProgress, label, sizeof(label));
    EXPECT(strcmp(label, "View Progress") == 0);
    dhikr_profile_action_label(&s.profiles[0], DhikrProfileActionReset, label, sizeof(label));
    EXPECT(strcmp(label, "Reset Progress") == 0);
}

static void test_view_progress_reads_chosen_profile(void) { // [P3] (9-12)
    DhikrState s;
    const uint8_t a = setup_reset_example(&s);
    DhikrState before = s;

    // The details are found by id, independent of the active profile.
    const int index = dhikr_state_find(&s, "c1");
    EXPECT_EQ_U(index, a);
    DhikrProgressText text;
    dhikr_progress_text(&s.profiles[index], &text);
    EXPECT(strcmp(text.current, "Current: 15/100") == 0);
    EXPECT(strcmp(text.sets, "Completed Sets: 4") == 0);
    EXPECT(strcmp(text.target, "Target: 100") == 0);

    dhikr_progress_text(&s.profiles[0], &text);
    EXPECT(strcmp(text.current, "Current: 20/33") == 0);
    EXPECT(strcmp(text.sets, "Completed Sets: 2") == 0);

    // Reading changes nothing, including the active profile.
    EXPECT(memcmp(&s, &before, sizeof(s)) == 0);
    EXPECT_EQ_U(s.active, 0);
}

static void test_counter_shows_percentage_only(void) { // [P4] (14-15)
    DhikrCounter c;
    dhikr_counter_init(&c, 33);
    c.count = 29;
    c.rounds = 5;
    DhikrBanner none = {0};
    char text[32];
    dhikr_counter_status_text(&c, &none, 0, text, sizeof(text));
    EXPECT(strcmp(text, "87%") == 0); // 29/33, rounded down as in v1.0.2
    EXPECT(strstr(text, "Set") == NULL);
    c.count = 0;
    dhikr_counter_status_text(&c, NULL, 0, text, sizeof(text));
    EXPECT(strcmp(text, "0%") == 0);
}

static void test_target_reached_message(void) { // [P5] (16-18)
    DhikrState s;
    dhikr_state_init_defaults(&s);
    DhikrBanner banner = {0};
    DhikrProfile* p = dhikr_state_active(&s);
    p->counter.count = 32;
    char text[32];

    DhikrActionOutcome o = dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 1000);
    EXPECT_EQ_U(o.feedback_event, DhikrFeedbackEventTarget);
    EXPECT_EQ_U(p->counter.count, 0); // automatic restart
    EXPECT_EQ_U(p->counter.rounds, 1); // exactly one set
    dhikr_counter_status_text(&p->counter, &banner, 1500, text, sizeof(text));
    EXPECT(strcmp(text, "Target reached!") == 0);
    // After ~1 s the bar shows the percentage of the new set again.
    dhikr_counter_status_text(&p->counter, &banner, 2000, text, sizeof(text));
    EXPECT(strcmp(text, "0%") == 0);

    // Undo still works: restores 32/33 and removes the set.
    dhikr_counter_apply(&s, &banner, DhikrCounterActionUndo, 2100);
    EXPECT_EQ_U(p->counter.count, 32);
    EXPECT_EQ_U(p->counter.rounds, 0);
    // Next count after undo: 33 -> set completed again, once.
    dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 2200);
    EXPECT_EQ_U(p->counter.rounds, 1);
    // Normal undo of a count.
    dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 2300);
    dhikr_counter_apply(&s, &banner, DhikrCounterActionUndo, 2400);
    EXPECT_EQ_U(p->counter.count, 0);
    dhikr_counter_status_text(&p->counter, &banner, 2500, text, sizeof(text));
    EXPECT(strcmp(text, "Undo: -1") == 0);
}

static void test_reset_inactive_profile_by_id(void) { // [P6] (19-22, 25)
    DhikrState s;
    const uint8_t a = setup_reset_example(&s);
    DhikrState before = s;

    EXPECT(dhikr_state_reset_profile(&s, s.profiles[a].id));

    // Istighfar: 15 -> 0, 4 sets -> 0, target 100, name and id kept.
    EXPECT_EQ_U(s.profiles[a].counter.count, 0);
    EXPECT_EQ_U(s.profiles[a].counter.rounds, 0);
    EXPECT_EQ_U(s.profiles[a].counter.target, 100);
    EXPECT(strcmp(s.profiles[a].name, "Istighfar") == 0);
    EXPECT(strcmp(s.profiles[a].id, "c1") == 0);
    // SubhanAllah (active) and every other profile untouched; active unchanged.
    EXPECT_EQ_U(s.active, 0);
    EXPECT_EQ_U(s.profiles[0].counter.count, 20);
    EXPECT_EQ_U(s.profiles[0].counter.rounds, 2);
    for(uint8_t i = 0; i < s.profile_count; i++) {
        if(i == a) continue;
        EXPECT(memcmp(&s.profiles[i], &before.profiles[i], sizeof(DhikrProfile)) == 0);
    }
    EXPECT_EQ_U(s.profile_count, before.profile_count);
    EXPECT(feedback_equal(&s.feedback, &before.feedback));

    // Unknown id: nothing changes.
    DhikrState after = s;
    EXPECT(!dhikr_state_reset_profile(&s, "c99"));
    EXPECT(!dhikr_state_reset_profile(&s, NULL));
    EXPECT(!dhikr_state_reset_profile(NULL, "c1"));
    EXPECT(memcmp(&s, &after, sizeof(s)) == 0);
}

static void test_reset_by_id_after_index_shift(void) { // [P7]
    // Indexes can move (a profile before it is deleted); the id still names
    // the right profile.
    DhikrState s;
    dhikr_state_init_defaults(&s);
    uint8_t x, y;
    dhikr_state_add_custom(&s, "First", 33, &x);
    dhikr_state_add_custom(&s, "Second", 33, &y);
    s.profiles[y].counter.count = 9;
    char id[DHIKR_PROFILE_ID_SIZE];
    memcpy(id, s.profiles[y].id, sizeof(id));
    EXPECT_EQ_U(dhikr_state_delete(&s, x), DhikrProfileOk);
    EXPECT(dhikr_state_reset_profile(&s, id));
    const int now = dhikr_state_find(&s, id);
    EXPECT(now >= 0);
    EXPECT(strcmp(s.profiles[now].name, "Second") == 0);
    EXPECT_EQ_U(s.profiles[now].counter.count, 0);
}

static void test_reset_cancel_changes_nothing(void) { // [P8] (23)
    // The scene resets only on DhikrConfirmAccepted; No, Back and the default
    // selection all cancel.
    DhikrState s;
    setup_reset_example(&s);
    const DhikrState before = s;
    const DhikrConfirmInput sequences[3][2] = {
        {DhikrConfirmInputOk, DhikrConfirmInputOk}, // default No
        {DhikrConfirmInputRight, DhikrConfirmInputBack}, // Yes highlighted, Back
        {DhikrConfirmInputBack, DhikrConfirmInputBack},
    };
    for(int i = 0; i < 3; i++) {
        DhikrConfirm confirm;
        dhikr_confirm_begin(&confirm);
        DhikrConfirmOutcome r = dhikr_confirm_handle(&confirm, sequences[i][0]);
        if(r == DhikrConfirmPending) r = dhikr_confirm_handle(&confirm, sequences[i][1]);
        EXPECT_EQ_U(r, DhikrConfirmCancelled);
        if(r == DhikrConfirmAccepted) dhikr_state_reset_profile(&s, "c1");
    }
    EXPECT(memcmp(&s, &before, sizeof(s)) == 0);
}

static void test_undo_cannot_restore_reset(void) { // [P9] (24)
    DhikrState s;
    dhikr_state_init_defaults(&s);
    DhikrBanner banner = {0};
    DhikrProfile* p = dhikr_state_active(&s);
    s.profiles[1].counter.count = 11;
    // Two completed sets, the second one just now (undo of that set possible).
    for(int i = 0; i < 66; i++) dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 0);
    EXPECT_EQ_U(p->counter.rounds, 2);
    EXPECT(p->counter.undo_round_available);

    EXPECT(dhikr_state_reset_profile(&s, p->id));
    EXPECT(!p->counter.undo_round_available);

    // Back on the counter screen: hold Down.
    DhikrActionOutcome o = dhikr_counter_apply(&s, &banner, DhikrCounterActionUndo, 100);
    EXPECT(!o.changed);
    EXPECT_EQ_U(banner.kind, DhikrBannerNothingToUndo);
    EXPECT_EQ_U(p->counter.count, 0);
    EXPECT_EQ_U(p->counter.rounds, 0);
    EXPECT_EQ_U(s.profiles[1].counter.count, 11);

    // Same for an inactive profile whose undo information was still pending.
    dhikr_state_select(&s, 1);
    s.profiles[1].counter.count = 32;
    dhikr_counter_apply(&s, &banner, DhikrCounterActionIncrement, 200); // completes a set
    dhikr_state_select(&s, 0);
    EXPECT(dhikr_state_reset_profile(&s, s.profiles[1].id));
    dhikr_state_select(&s, 1);
    dhikr_counter_apply(&s, &banner, DhikrCounterActionUndo, 300);
    EXPECT_EQ_U(s.profiles[1].counter.count, 0);
    EXPECT_EQ_U(s.profiles[1].counter.rounds, 0);
}

static void test_reset_persisted_and_usable(void) { // [P10] (26-27)
    fake_reset();
    DhikrState s, loaded;
    const uint8_t a = setup_reset_example(&s);
    EXPECT(dhikr_state_reset_profile(&s, "c1"));
    EXPECT_EQ_U(save_and_reload(&s, &loaded), DhikrLoadOk);
    EXPECT(states_equal(&s, &loaded));
    EXPECT_EQ_U(loaded.profiles[a].counter.count, 0);
    EXPECT_EQ_U(loaded.profiles[a].counter.rounds, 0);
    EXPECT_EQ_U(loaded.profiles[a].counter.target, 100);
    EXPECT_EQ_U(loaded.profiles[0].counter.count, 20);
    EXPECT_EQ_U(loaded.active, 0);

    // The reset profile counts normally afterwards.
    DhikrBanner banner = {0};
    dhikr_state_select(&loaded, a);
    for(int i = 0; i < 101; i++) {
        dhikr_counter_apply(&loaded, &banner, DhikrCounterActionIncrement, 0);
    }
    EXPECT_EQ_U(loaded.profiles[a].counter.count, 1);
    EXPECT_EQ_U(loaded.profiles[a].counter.rounds, 1);
}

static void test_reset_save_failure_consistent(void) { // [P11] (31)
    fake_reset();
    DhikrState s, loaded;
    setup_reset_example(&s);
    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_save(&store, &s), DhikrSaveWritten);

    // The card refuses the write: the old file stays complete and valid.
    EXPECT(dhikr_state_reset_profile(&s, "c1"));
    g_fs.write_error = true;
    EXPECT_EQ_U(dhikr_store_save(&store, &s), DhikrSaveFailed);
    DhikrStore probe;
    dhikr_store_init(&probe, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&probe, &loaded), DhikrLoadOk);
    EXPECT_EQ_U(loaded.profiles[4].counter.count, 15); // previous state, not half-written
    EXPECT_EQ_U(loaded.profiles[4].counter.rounds, 4);

    // The next attempt (the app keeps the change dirty) stores the reset.
    g_fs.write_error = false;
    EXPECT_EQ_U(dhikr_store_save(&store, &s), DhikrSaveWritten);
    dhikr_store_init(&probe, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&probe, &loaded), DhikrLoadOk);
    EXPECT_EQ_U(loaded.profiles[4].counter.count, 0);
    EXPECT_EQ_U(loaded.profiles[4].counter.rounds, 0);
    EXPECT(states_equal(&s, &loaded));
}

static void test_crc32_known_vector(void) {
    EXPECT_EQ_U(dhikr_crc32("123456789", 9), 0xCBF43926UL);
}

static void test_edit_profiles_labels_names_only(void) { // [P12]
    DhikrState s;
    setup_reset_example(&s);
    DhikrState before = s;
    char label[40];
    for(uint8_t i = 0; i < s.profile_count; i++) {
        const DhikrProfile* p = &s.profiles[i];
        dhikr_profile_list_label(p, label, sizeof(label));
        // Exactly the name, no "(fixed)" marker, for built-in and custom profiles.
        EXPECT(strcmp(label, p->name) == 0);
        EXPECT(strstr(label, "fixed") == NULL);
        EXPECT(strchr(label, '(') == NULL);
        // Permissions are unchanged.
        EXPECT_EQ_U(dhikr_profile_action_allowed(p, DhikrProfileActionRename), !p->builtin);
        EXPECT_EQ_U(dhikr_profile_action_allowed(p, DhikrProfileActionDelete), !p->builtin);
        EXPECT(dhikr_profile_action_allowed(p, DhikrProfileActionProgress));
        EXPECT(dhikr_profile_action_allowed(p, DhikrProfileActionTarget));
        EXPECT(dhikr_profile_action_allowed(p, DhikrProfileActionReset));
    }
    dhikr_profile_list_label(&s.profiles[0], label, sizeof(label));
    EXPECT(strcmp(label, "SubhanAllah") == 0);
    EXPECT(s.profiles[0].builtin);

    // Longest possible name fits completely; a small buffer truncates safely.
    DhikrProfile longp = s.profiles[s.profile_count - 1];
    memset(longp.name, 'W', DHIKR_PROFILE_NAME_SIZE - 1);
    longp.name[DHIKR_PROFILE_NAME_SIZE - 1] = '\0';
    dhikr_profile_list_label(&longp, label, sizeof(label));
    EXPECT_EQ_U(strlen(label), DHIKR_PROFILE_NAME_SIZE - 1);
    char small[5];
    dhikr_profile_list_label(&longp, small, sizeof(small));
    EXPECT(strcmp(small, "WWWW") == 0);

    // Building labels changes no profile data.
    EXPECT(memcmp(&s, &before, sizeof(s)) == 0);
}

int main(void) {
    printf("Flipper Dhikr host unit tests\n");

    // Still-required v1.0 behaviour [R20]
    RUN_TEST(test_increment_from_zero_to_one);
    RUN_TEST(test_custom_target);
    RUN_TEST(test_invalid_targets_rejected);
    RUN_TEST(test_target_change_keeps_count);
    RUN_TEST(test_progress);
    RUN_TEST(test_counter_never_overflows);
    RUN_TEST(test_reset_confirmed);
    RUN_TEST(test_reset_cancelled);
    RUN_TEST(test_defaults);
    RUN_TEST(test_codec_roundtrip);
    RUN_TEST(test_store_missing_file);
    RUN_TEST(test_store_corrupt_file);
    RUN_TEST(test_store_skips_unchanged_writes);
    RUN_TEST(test_codec_detects_tampering);
    RUN_TEST(test_codec_rejects_invalid_values);
    RUN_TEST(test_codec_forward_compatible);
    RUN_TEST(test_store_unsupported_version);
    RUN_TEST(test_store_read_error_protects_file);
    RUN_TEST(test_store_oversized_file);
    RUN_TEST(test_crc32_known_vector);

    // Undo [R1-R3]
    RUN_TEST(test_undo_at_ten);
    RUN_TEST(test_undo_at_zero);
    RUN_TEST(test_long_down_undoes_once);
    RUN_TEST(test_undo_after_completed_round);
    RUN_TEST(test_undo_only_active_profile_and_saved);

    // Automatic restart [R4-R8]
    RUN_TEST(test_auto_reset_33);
    RUN_TEST(test_auto_reset_99);
    RUN_TEST(test_auto_reset_1);
    RUN_TEST(test_target_feedback_exactly_once_per_round);
    RUN_TEST(test_input_during_target_banner);
    RUN_TEST(test_state_consistent_when_leaving_during_banner);

    // Profiles [R9-R15]
    RUN_TEST(test_create_custom_profile);
    RUN_TEST(test_manage_multiple_custom_profiles);
    RUN_TEST(test_rename_custom_profile);
    RUN_TEST(test_delete_custom_profile);
    RUN_TEST(test_builtin_profiles_protected);
    RUN_TEST(test_profile_limit);
    RUN_TEST(test_active_marker_and_persistence);

    // Storage [R16-R18]
    RUN_TEST(test_migrate_v1_file);
    RUN_TEST(test_migrate_v1_without_custom_name);
    RUN_TEST(test_persist_custom_profiles);
    RUN_TEST(test_interrupted_save);

    // v1.0.2 independent feedback [F1-F13]; F14 = all tests above still passing
    RUN_TEST(test_feedback_defaults);
    RUN_TEST(test_feedback_each_switch);
    RUN_TEST(test_feedback_named_combinations);
    RUN_TEST(test_feedback_all_16_combinations);
    RUN_TEST(test_feedback_no_double_on_target);
    RUN_TEST(test_feedback_migration_from_v2);
    RUN_TEST(test_feedback_migration_chain_from_v1);
    RUN_TEST(test_feedback_corrupt_files);

    // v1.0.3 menu and UX [U1-U12]
    RUN_TEST(test_main_menu_without_target_settings);
    RUN_TEST(test_edit_target_reachable_for_every_profile);
    RUN_TEST(test_edit_inactive_profile_keeps_active);
    RUN_TEST(test_set_target_rejects_invalid);
    RUN_TEST(test_profile_selection_and_marker);
    RUN_TEST(test_builtin_actions_restricted);
    RUN_TEST(test_custom_profile_actions);
    RUN_TEST(test_sets_persist_after_restart);
    RUN_TEST(test_set_banner_and_undo);
    RUN_TEST(test_target_change_keeps_counts_and_sets);
    RUN_TEST(test_v1_0_2_settings_compatible);
    RUN_TEST(test_feedback_unchanged_by_ux_update);

    // v1.0.3 profile details, View Progress, per-profile reset [P1-P11]
    RUN_TEST(test_main_menu_entries_unique);
    RUN_TEST(test_profile_details_actions);
    RUN_TEST(test_view_progress_reads_chosen_profile);
    RUN_TEST(test_counter_shows_percentage_only);
    RUN_TEST(test_target_reached_message);
    RUN_TEST(test_reset_inactive_profile_by_id);
    RUN_TEST(test_reset_by_id_after_index_shift);
    RUN_TEST(test_reset_cancel_changes_nothing);
    RUN_TEST(test_undo_cannot_restore_reset);
    RUN_TEST(test_reset_persisted_and_usable);
    RUN_TEST(test_reset_save_failure_consistent);
    RUN_TEST(test_edit_profiles_labels_names_only);

    printf("\n%d tests, %d failed\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
