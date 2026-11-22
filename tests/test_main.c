/*
 * Host unit tests for the hardware-independent Flipper Dhikr logic.
 * Build and run with `make -C tests` (see tests/Makefile).
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "include/confirm.h"
#include "include/counter.h"
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

#define FAKE_FILES 4
#define FAKE_FILE_SIZE 4096

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
       a->feedback != b->feedback) {
        return false;
    }
    for(uint8_t i = 0; i < a->profile_count; i++) {
        const DhikrProfile* pa = &a->profiles[i];
        const DhikrProfile* pb = &b->profiles[i];
        if(strcmp(pa->id, pb->id) != 0 || strcmp(pa->name, pb->name) != 0 ||
           pa->counter.count != pb->counter.count || pa->counter.target != pb->counter.target ||
           pa->name_editable != pb->name_editable) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------------- */
/* 1-4, 11, 12: counter                                                      */
/* ------------------------------------------------------------------------- */

static void test_increment_from_zero_to_one(void) {
    DhikrCounter c;
    dhikr_counter_init(&c, 33);
    EXPECT_EQ_U(c.count, 0);
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementCounted);
    EXPECT_EQ_U(c.count, 1);
    EXPECT_EQ_U(c.target, 33);
}

static void reach_target(uint32_t target) {
    DhikrCounter c;
    dhikr_counter_init(&c, target);
    for(uint32_t i = 1; i < target; i++) {
        EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementCounted);
        EXPECT(!dhikr_counter_is_complete(&c));
    }
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementTargetReached);
    EXPECT_EQ_U(c.count, target);
    EXPECT(dhikr_counter_is_complete(&c));
    EXPECT_EQ_U(dhikr_counter_progress_permille(&c), 1000);
}

static void test_reach_target_33(void) {
    reach_target(DHIKR_TARGET_PRESET_A);
}

static void test_reach_target_99(void) {
    reach_target(DHIKR_TARGET_PRESET_B);
}

static void test_custom_target(void) {
    DhikrCounter c;
    dhikr_counter_init(&c, 33);
    EXPECT(dhikr_counter_set_target(&c, 7));
    EXPECT_EQ_U(c.target, 7);
    reach_target(7);
    reach_target(1);
    reach_target(DHIKR_TARGET_MAX);
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
    // Lowering the target below the count completes silently: no later "reached" event.
    EXPECT(dhikr_counter_set_target(&c, 10));
    EXPECT_EQ_U(c.count, 20);
    EXPECT(dhikr_counter_is_complete(&c));
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementCounted);
}

static void test_target_feedback_only_once(void) {
    DhikrCounter c;
    dhikr_counter_init(&c, 3);
    int reached = 0;
    for(int i = 0; i < 100; i++) {
        if(dhikr_counter_increment(&c) == DhikrIncrementTargetReached) reached++;
    }
    EXPECT_EQ_U(reached, 1);
    EXPECT_EQ_U(c.count, 100);
    // After a reset the target can be reached (and announced) again.
    dhikr_counter_reset(&c);
    reached = 0;
    for(int i = 0; i < 10; i++) {
        if(dhikr_counter_increment(&c) == DhikrIncrementTargetReached) reached++;
    }
    EXPECT_EQ_U(reached, 1);
}

static void test_counter_max_saturates(void) {
    DhikrCounter c;
    dhikr_counter_init(&c, 33);
    c.count = DHIKR_COUNT_MAX - 1;
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementCounted);
    EXPECT_EQ_U(c.count, DHIKR_COUNT_MAX);
    EXPECT(dhikr_counter_is_saturated(&c));
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementSaturated);
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementSaturated);
    EXPECT_EQ_U(c.count, DHIKR_COUNT_MAX);
    EXPECT_EQ_U(dhikr_counter_progress_permille(&c), 1000);
    EXPECT_EQ_U(dhikr_counter_increment(NULL), DhikrIncrementSaturated);

    // A corrupted in-memory value above the max is never incremented further.
    c.count = UINT32_MAX;
    EXPECT_EQ_U(dhikr_counter_increment(&c), DhikrIncrementSaturated);
    EXPECT_EQ_U(c.count, UINT32_MAX);
    EXPECT(!dhikr_counter_is_valid(&c));
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

/* ------------------------------------------------------------------------- */
/* 6, 7: reset confirmation                                                   */
/* ------------------------------------------------------------------------- */

static void test_reset_confirmed(void) {
    DhikrState s;
    dhikr_state_init_defaults(&s);
    s.profiles[0].counter.count = 15;
    s.profiles[1].counter.count = 40;

    DhikrConfirm confirm;
    dhikr_confirm_begin(&confirm);
    EXPECT_EQ_U(confirm.selection, DhikrConfirmChoiceNo);
    EXPECT_EQ_U(dhikr_confirm_handle(&confirm, DhikrConfirmInputRight), DhikrConfirmPending);
    EXPECT_EQ_U(confirm.selection, DhikrConfirmChoiceYes);
    EXPECT_EQ_U(dhikr_confirm_handle(&confirm, DhikrConfirmInputOk), DhikrConfirmAccepted);

    dhikr_state_reset_active(&s);
    EXPECT_EQ_U(s.profiles[0].counter.count, 0);
    EXPECT_EQ_U(s.profiles[1].counter.count, 40); // only the active profile
    EXPECT_EQ_U(s.profiles[0].counter.target, DHIKR_TARGET_DEFAULT);
}

static void test_reset_cancelled(void) {
    DhikrConfirm confirm;

    // OK on the default selection ("No") cancels.
    dhikr_confirm_begin(&confirm);
    EXPECT_EQ_U(dhikr_confirm_handle(&confirm, DhikrConfirmInputOk), DhikrConfirmCancelled);

    // Back cancels even when "Yes" is highlighted.
    dhikr_confirm_begin(&confirm);
    dhikr_confirm_handle(&confirm, DhikrConfirmInputRight);
    EXPECT_EQ_U(dhikr_confirm_handle(&confirm, DhikrConfirmInputBack), DhikrConfirmCancelled);

    // Moving back to "No" and confirming cancels.
    dhikr_confirm_begin(&confirm);
    dhikr_confirm_handle(&confirm, DhikrConfirmInputRight);
    dhikr_confirm_handle(&confirm, DhikrConfirmInputLeft);
    EXPECT_EQ_U(dhikr_confirm_handle(&confirm, DhikrConfirmInputOk), DhikrConfirmCancelled);

    // A new dialog always starts at "No".
    dhikr_confirm_begin(&confirm);
    dhikr_confirm_handle(&confirm, DhikrConfirmInputRight);
    dhikr_confirm_begin(&confirm);
    EXPECT_EQ_U(confirm.selection, DhikrConfirmChoiceNo);
}

/* ------------------------------------------------------------------------- */
/* 8: profiles                                                               */
/* ------------------------------------------------------------------------- */

static void test_profile_switch_keeps_counts(void) {
    DhikrState s;
    dhikr_state_init_defaults(&s);
    EXPECT_EQ_U(s.profile_count, 4);
    EXPECT(strcmp(s.profiles[0].name, "SubhanAllah") == 0);
    EXPECT(strcmp(s.profiles[3].name, "Custom Dhikr") == 0);

    for(int i = 0; i < 5; i++)
        dhikr_counter_increment(&dhikr_state_active(&s)->counter);
    EXPECT(dhikr_state_select(&s, 2));
    for(int i = 0; i < 7; i++)
        dhikr_counter_increment(&dhikr_state_active(&s)->counter);
    dhikr_state_cycle(&s, 1); // -> 3
    EXPECT_EQ_U(s.active, 3);
    dhikr_state_cycle(&s, 1); // wraps -> 0
    EXPECT_EQ_U(s.active, 0);
    dhikr_state_cycle(&s, -1); // wraps -> 3
    EXPECT_EQ_U(s.active, 3);
    EXPECT(!dhikr_state_select(&s, 4));
    EXPECT_EQ_U(s.active, 3);

    EXPECT_EQ_U(s.profiles[0].counter.count, 5);
    EXPECT_EQ_U(s.profiles[1].counter.count, 0);
    EXPECT_EQ_U(s.profiles[2].counter.count, 7);
    EXPECT_EQ_U(s.profiles[3].counter.count, 0);
    EXPECT_EQ_U(dhikr_state_find(&s, "custom"), 3);
    EXPECT(dhikr_state_find(&s, "nope") < 0);
}

static void test_profile_rename(void) {
    DhikrState s;
    dhikr_state_init_defaults(&s);
    EXPECT(!dhikr_profile_rename(&s.profiles[0], "Other")); // built-in names are fixed
    EXPECT(dhikr_profile_rename(&s.profiles[3], "  La ilaha  "));
    EXPECT(strcmp(s.profiles[3].name, "La ilaha") == 0);
    EXPECT(!dhikr_profile_rename(&s.profiles[3], "   "));
    EXPECT(!dhikr_profile_rename(&s.profiles[3], ""));
    EXPECT(!dhikr_profile_rename(&s.profiles[3], "12345678901234567")); // 17 chars
    EXPECT(!dhikr_profile_rename(&s.profiles[3], "bad\nname"));
    EXPECT(strcmp(s.profiles[3].name, "La ilaha") == 0);
    EXPECT(dhikr_profile_rename(&s.profiles[3], "1234567890123456")); // 16 chars
}

/* ------------------------------------------------------------------------- */
/* 9, 10: persistence                                                        */
/* ------------------------------------------------------------------------- */

static void make_sample_state(DhikrState* s) {
    dhikr_state_init_defaults(s);
    s->profiles[0].counter.count = 12;
    s->profiles[1].counter.count = 99;
    s->profiles[1].counter.target = 99;
    s->profiles[2].counter.count = DHIKR_COUNT_MAX;
    s->profiles[3].counter.target = 1234;
    dhikr_profile_rename(&s->profiles[3], "My Dhikr = 1");
    s->active = 3;
    s->feedback = DhikrFeedbackBoth;
}

static void test_codec_roundtrip(void) {
    DhikrState s, loaded;
    make_sample_state(&s);
    char buffer[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t length = dhikr_settings_serialize(&s, buffer, sizeof(buffer));
    EXPECT(length > 0);
    EXPECT_EQ_U(dhikr_settings_parse(buffer, length, &loaded), DhikrCodecOk);
    EXPECT(states_equal(&s, &loaded));

    // Too small buffer fails cleanly.
    char tiny[16];
    EXPECT_EQ_U(dhikr_settings_serialize(&s, tiny, sizeof(tiny)), 0);
}

static void test_store_save_and_restore(void) {
    fake_reset();
    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    DhikrState s, loaded;
    make_sample_state(&s);

    EXPECT_EQ_U(dhikr_store_save(&store, &s), DhikrSaveWritten);
    EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_PATH) != NULL);
    EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_TMP_PATH) == NULL);

    // Fresh "app start".
    DhikrStore store2;
    dhikr_store_init(&store2, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store2, &loaded), DhikrLoadOk);
    EXPECT(states_equal(&s, &loaded));
    EXPECT_EQ_U(loaded.feedback, DhikrFeedbackBoth);
    EXPECT_EQ_U(loaded.active, 3);
    EXPECT(strcmp(loaded.profiles[3].name, "My Dhikr = 1") == 0);
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

    // After a load, saving the identical state does not write.
    DhikrStore store2;
    DhikrState loaded;
    dhikr_store_init(&store2, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store2, &loaded), DhikrLoadOk);
    EXPECT_EQ_U(dhikr_store_save(&store2, &loaded), DhikrSaveUnchanged);
    EXPECT_EQ_U(g_fs.write_count, 2);
}

static void test_store_missing_file(void) {
    fake_reset();
    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    DhikrState loaded, defaults;
    dhikr_state_init_defaults(&defaults);
    loaded.active = 77; // garbage must be replaced

    EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadMissing);
    EXPECT(states_equal(&loaded, &defaults));
    EXPECT(!store.read_only);
    EXPECT_EQ_U(dhikr_store_save(&store, &loaded), DhikrSaveWritten); // first-start defaults
}

static void test_store_corrupt_file(void) {
    static const char* bad_files[] = {
        "",
        "garbage",
        "FlipperDhikr\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\n", // no checksum
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
        // The damaged file is preserved as a backup, not overwritten.
        EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_BACKUP_PATH) != NULL);
        EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_PATH) == NULL);
    }
}

static void test_codec_detects_tampering(void) {
    DhikrState s, loaded;
    make_sample_state(&s);
    char buffer[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t length = dhikr_settings_serialize(&s, buffer, sizeof(buffer));

    // Flip one digit in the body -> checksum mismatch.
    char* p = strstr(buffer, "count=12");
    EXPECT(p != NULL);
    p[6] = '9';
    EXPECT_EQ_U(dhikr_settings_parse(buffer, length, &loaded), DhikrCodecCorrupt);
    EXPECT_EQ_U(loaded.profiles[0].counter.count, 0); // defaults

    // Truncation -> corrupt.
    length = dhikr_settings_serialize(&s, buffer, sizeof(buffer));
    EXPECT_EQ_U(dhikr_settings_parse(buffer, length / 2, &loaded), DhikrCodecCorrupt);
}

/* Build a file with a correct checksum around an arbitrary body. */
static size_t build_file(char* out, size_t capacity, const char* body) {
    int n = snprintf(out, capacity, "%s", body);
    uint32_t crc = dhikr_crc32(out, (size_t)n);
    n += snprintf(out + n, capacity - (size_t)n, "crc32=%08lX\n", (unsigned long)crc);
    return (size_t)n;
}

static void test_codec_rejects_invalid_values(void) {
    static const char* bodies[] = {
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=9\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.subhanallah.target=0\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.subhanallah.target=10000\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.subhanallah.count=1000000\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.subhanallah.count=-1\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.subhanallah.count=99999999999\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.custom.name=\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nprofile.custom.name=12345678901234567\n",
        "FlipperDhikr\nversion=1\nactive=subhanallah\n", // missing feedback
        "FlipperDhikr\nversion=1\nactive=subhanallah\nfeedback=1\nnot a key value line\n",
    };
    char buffer[DHIKR_SETTINGS_BUFFER_SIZE];
    DhikrState loaded;
    for(size_t i = 0; i < sizeof(bodies) / sizeof(bodies[0]); i++) {
        size_t length = build_file(buffer, sizeof(buffer), bodies[i]);
        DhikrCodecResult result = dhikr_settings_parse(buffer, length, &loaded);
        if(result != DhikrCodecCorrupt) printf("    body %zu parsed unexpectedly\n", i);
        EXPECT_EQ_U(result, DhikrCodecCorrupt);
    }
}

static void test_codec_forward_compatible(void) {
    // Unknown keys/profiles are ignored; missing profiles keep defaults; CRLF accepted.
    const char* body = "FlipperDhikr\r\nversion=1\r\nactive=custom\r\nfeedback=2\r\n"
                       "future.option=42\r\n"
                       "profile.removed_profile.count=5\r\n"
                       "profile.alhamdulillah.count=8\r\n"
                       "profile.alhamdulillah.color=green\r\n";
    char buffer[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t length = build_file(buffer, sizeof(buffer), body);
    DhikrState loaded;
    EXPECT_EQ_U(dhikr_settings_parse(buffer, length, &loaded), DhikrCodecOk);
    EXPECT_EQ_U(loaded.active, 3);
    EXPECT_EQ_U(loaded.feedback, DhikrFeedbackSound);
    EXPECT_EQ_U(loaded.profiles[1].counter.count, 8);
    EXPECT_EQ_U(loaded.profiles[0].counter.count, 0);
    EXPECT_EQ_U(loaded.profiles[0].counter.target, DHIKR_TARGET_DEFAULT);
}

static void test_store_unsupported_version(void) {
    fake_reset();
    const char* future = "FlipperDhikr\nversion=2\nsomething new\n";
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
    make_sample_state(&s);
    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_save(&store, &s), DhikrSaveWritten);

    g_fs.read_error = true;
    DhikrStore store2;
    dhikr_store_init(&store2, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store2, &loaded), DhikrLoadIoError);
    EXPECT(store2.read_only);
    // Saving defaults must not overwrite the possibly intact file.
    EXPECT_EQ_U(dhikr_store_save(&store2, &loaded), DhikrSaveFailed);
    g_fs.read_error = false;

    DhikrStore store3;
    dhikr_store_init(&store3, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store3, &loaded), DhikrLoadOk);
    EXPECT(states_equal(&s, &loaded));
}

static void test_store_write_failures(void) {
    fake_reset();
    DhikrState s, loaded;
    make_sample_state(&s);
    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_save(&store, &s), DhikrSaveWritten);

    // Write failure keeps the previous file intact.
    DhikrState changed = s;
    changed.profiles[0].counter.count = 500;
    g_fs.write_error = true;
    EXPECT_EQ_U(dhikr_store_save(&store, &changed), DhikrSaveFailed);
    g_fs.write_error = false;
    DhikrStore store2;
    dhikr_store_init(&store2, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store2, &loaded), DhikrLoadOk);
    EXPECT_EQ_U(loaded.profiles[0].counter.count, 12);

    // A failed save is retried later (it is not mistaken for "unchanged").
    EXPECT_EQ_U(dhikr_store_save(&store, &changed), DhikrSaveWritten);
}

static void test_store_recovers_temp_file(void) {
    // Simulate an interruption after the temp file was written but before the rename.
    fake_reset();
    DhikrState s, loaded;
    make_sample_state(&s);
    char buffer[DHIKR_SETTINGS_BUFFER_SIZE];
    size_t length = dhikr_settings_serialize(&s, buffer, sizeof(buffer));
    fake_put(&g_fs, DHIKR_SETTINGS_TMP_PATH, buffer, length);

    DhikrStore store;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadRecovered);
    EXPECT(states_equal(&s, &loaded));
    EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_PATH) != NULL);
    EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_TMP_PATH) == NULL);

    // An incomplete temp file is discarded.
    fake_reset();
    fake_put(&g_fs, DHIKR_SETTINGS_TMP_PATH, buffer, length / 2);
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadMissing);
    EXPECT(fake_find(&g_fs, DHIKR_SETTINGS_TMP_PATH) == NULL);
    EXPECT_EQ_U(loaded.profiles[0].counter.count, 0);
}

static void test_store_oversized_file(void) {
    fake_reset();
    char big[FAKE_FILE_SIZE];
    memset(big, 'A', sizeof(big));
    fake_put(&g_fs, DHIKR_SETTINGS_PATH, big, sizeof(big));
    DhikrStore store;
    DhikrState loaded;
    dhikr_store_init(&store, &g_io);
    EXPECT_EQ_U(dhikr_store_load(&store, &loaded), DhikrLoadCorrupt);
}

/* ------------------------------------------------------------------------- */
/* Feedback modes                                                            */
/* ------------------------------------------------------------------------- */

static void test_feedback_modes(void) {
    EXPECT(!dhikr_feedback_mode_uses_vibro(DhikrFeedbackOff));
    EXPECT(!dhikr_feedback_mode_uses_sound(DhikrFeedbackOff));
    EXPECT(dhikr_feedback_mode_uses_vibro(DhikrFeedbackVibro));
    EXPECT(!dhikr_feedback_mode_uses_sound(DhikrFeedbackVibro));
    EXPECT(!dhikr_feedback_mode_uses_vibro(DhikrFeedbackSound));
    EXPECT(dhikr_feedback_mode_uses_sound(DhikrFeedbackSound));
    EXPECT(dhikr_feedback_mode_uses_vibro(DhikrFeedbackBoth));
    EXPECT(dhikr_feedback_mode_uses_sound(DhikrFeedbackBoth));
    EXPECT(!dhikr_feedback_mode_is_valid(DhikrFeedbackModeCount));
    EXPECT(strcmp(dhikr_feedback_mode_name(DhikrFeedbackBoth), "Both") == 0);
    EXPECT(strcmp(dhikr_feedback_mode_name((DhikrFeedbackMode)42), "?") == 0);
}

static void test_crc32_known_vector(void) {
    EXPECT_EQ_U(dhikr_crc32("123456789", 9), 0xCBF43926UL);
}

int main(void) {
    printf("Flipper Dhikr host unit tests\n");

    RUN_TEST(test_increment_from_zero_to_one);        // 1
    RUN_TEST(test_reach_target_33);                   // 2
    RUN_TEST(test_reach_target_99);                   // 3
    RUN_TEST(test_custom_target);                     // 4
    RUN_TEST(test_invalid_targets_rejected);          // 5
    RUN_TEST(test_reset_confirmed);                   // 6
    RUN_TEST(test_reset_cancelled);                   // 7
    RUN_TEST(test_profile_switch_keeps_counts);       // 8
    RUN_TEST(test_codec_roundtrip);                   // 9
    RUN_TEST(test_store_save_and_restore);            // 9
    RUN_TEST(test_store_missing_file);                // 10
    RUN_TEST(test_store_corrupt_file);                // 10
    RUN_TEST(test_target_feedback_only_once);         // 11
    RUN_TEST(test_counter_max_saturates);             // 12
    RUN_TEST(test_target_change_keeps_count);
    RUN_TEST(test_progress);
    RUN_TEST(test_profile_rename);
    RUN_TEST(test_store_skips_unchanged_writes);
    RUN_TEST(test_codec_detects_tampering);
    RUN_TEST(test_codec_rejects_invalid_values);
    RUN_TEST(test_codec_forward_compatible);
    RUN_TEST(test_store_unsupported_version);
    RUN_TEST(test_store_read_error_protects_file);
    RUN_TEST(test_store_write_failures);
    RUN_TEST(test_store_recovers_temp_file);
    RUN_TEST(test_store_oversized_file);
    RUN_TEST(test_feedback_modes);
    RUN_TEST(test_crc32_known_vector);

    printf("\n%d tests, %d failed\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
