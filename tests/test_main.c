/*
 * Host unit tests for the hardware-independent Flipper Dhikr logic.
 * Build and run with `make -C tests` (see tests/Makefile).
 *
 * Tests marked [R<n>] cover requirement n of the v1.0.1 test list.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "include/confirm.h"
#include "include/counter.h"
#include "include/counter_input.h"
#include "include/feedback.h"
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

static bool states_equal(const DhikrState* a, const DhikrState* b) {
    if(a->profile_count != b->profile_count || a->active != b->active ||
       a->feedback != b->feedback || a->next_custom_id != b->next_custom_id) {
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
    EXPECT_EQ_U(s->feedback, DhikrFeedbackBoth);
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
    EXPECT(main_file && strstr(main_file->data, "version=2\n") != NULL);

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
    EXPECT_EQ_U(s.feedback, DhikrFeedbackOff);
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
    s.feedback = DhikrFeedbackSound;

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
    s.feedback = DhikrFeedbackBoth;
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
    EXPECT_EQ_U(loaded.feedback, DhikrFeedbackSound);
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
    const char* future = "FlipperDhikr\nversion=3\nsomething new\n";
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
/* [R19] Feedback modes                                                      */
/* ========================================================================= */

static void test_feedback_modes(void) {
    const struct {
        DhikrFeedbackMode mode;
        bool vibro;
        bool sound;
    } cases[] = {
        {DhikrFeedbackOff, false, false},
        {DhikrFeedbackVibro, true, false},
        {DhikrFeedbackSound, false, true},
        {DhikrFeedbackBoth, true, true},
    };
    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        DhikrFeedbackPlan count = dhikr_feedback_plan(cases[i].mode, DhikrFeedbackEventCount);
        DhikrFeedbackPlan target = dhikr_feedback_plan(cases[i].mode, DhikrFeedbackEventTarget);
        EXPECT_EQ_U(count.vibro, cases[i].vibro);
        EXPECT_EQ_U(count.sound, cases[i].sound);
        EXPECT(!count.distinct && !count.led);
        EXPECT_EQ_U(target.vibro, cases[i].vibro);
        EXPECT_EQ_U(target.sound, cases[i].sound);
        bool on = cases[i].vibro || cases[i].sound;
        EXPECT_EQ_U(target.distinct, on); // distinct pattern unless Off
        EXPECT_EQ_U(target.led, on);
    }
    DhikrFeedbackPlan invalid = dhikr_feedback_plan((DhikrFeedbackMode)9, DhikrFeedbackEventTarget);
    EXPECT(!invalid.vibro && !invalid.sound && !invalid.led);
    EXPECT(strcmp(dhikr_feedback_mode_name(DhikrFeedbackBoth), "Both") == 0);
    EXPECT(strcmp(dhikr_feedback_mode_name((DhikrFeedbackMode)42), "?") == 0);
}

static void test_crc32_known_vector(void) {
    EXPECT_EQ_U(dhikr_crc32("123456789", 9), 0xCBF43926UL);
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

    // Feedback [R19]
    RUN_TEST(test_feedback_modes);

    printf("\n%d tests, %d failed\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
