/**
 * @file storage.h
 * @brief Load/save of DhikrState with crash-safe writes.
 *
 * The file system is accessed through the small DhikrFileIo interface so the
 * load/save policy can be unit tested on the host with an in-memory fake.
 * The Flipper implementation is provided by storage_flipper.h.
 *
 * Save policy:
 *  - Serialize; if the content equals the last loaded/saved content, skip
 *    the write entirely (no unnecessary SD card writes).
 *  - Write to a temporary file, then rename it over the settings file, so an
 *    interrupted write never destroys the previous good file.
 *
 * Load policy:
 *  - Valid file           -> use it.
 *  - Missing file         -> try a leftover temporary file, otherwise defaults.
 *  - Corrupt/unsupported  -> move it to a backup file, use defaults.
 *  - I/O error            -> defaults and saving is disabled for this session
 *                            so a possibly intact file is never overwritten.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "include/profiles.h"

#ifdef __cplusplus
extern "C" {
#endif

/* "/data" is resolved by the Flipper storage service to /ext/apps_data/<appid>. */
#define DHIKR_SETTINGS_DIR "/data"
#define DHIKR_SETTINGS_PATH DHIKR_SETTINGS_DIR "/settings.txt"
#define DHIKR_SETTINGS_TMP_PATH DHIKR_SETTINGS_DIR "/settings.tmp"
#define DHIKR_SETTINGS_BACKUP_PATH DHIKR_SETTINGS_DIR "/settings.bak"

typedef enum {
    DhikrIoOk,
    DhikrIoNotFound,
    DhikrIoError,
} DhikrIoStatus;

typedef struct {
    void* context;
    /** Read up to `capacity` bytes. A larger file yields exactly `capacity` bytes. */
    DhikrIoStatus (*read)(void* context, const char* path, char* buffer, size_t capacity, size_t* length);
    /** Create or truncate `path` and write `length` bytes. */
    DhikrIoStatus (*write)(void* context, const char* path, const char* data, size_t length);
    /** Rename, replacing an existing destination. */
    DhikrIoStatus (*rename)(void* context, const char* from, const char* to);
    /** Remove a file. A missing file returns DhikrIoNotFound. */
    DhikrIoStatus (*remove)(void* context, const char* path);
} DhikrFileIo;

typedef enum {
    DhikrLoadOk,
    /** Settings file was missing but a complete temporary file was recovered. */
    DhikrLoadRecovered,
    /** No settings yet (first start): defaults are used. */
    DhikrLoadMissing,
    /** File was damaged; it was moved to the backup path, defaults are used. */
    DhikrLoadCorrupt,
    /** File is from a newer app version; moved to the backup path, defaults used. */
    DhikrLoadUnsupported,
    /** Storage not readable; defaults used and saving disabled. */
    DhikrLoadIoError,
} DhikrLoadResult;

typedef enum {
    DhikrSaveWritten,
    DhikrSaveUnchanged,
    DhikrSaveFailed,
} DhikrSaveResult;

typedef struct {
    const DhikrFileIo* io;
    uint32_t last_crc;
    bool has_last_crc;
    /** Set after a load I/O error to protect an existing file. */
    bool read_only;
} DhikrStore;

void dhikr_store_init(DhikrStore* store, const DhikrFileIo* io);

/** Always leaves a valid state in `out` (defaults on any failure). */
DhikrLoadResult dhikr_store_load(DhikrStore* store, DhikrState* out);

/** Save if the content changed since the last load/save. */
DhikrSaveResult dhikr_store_save(DhikrStore* store, const DhikrState* state);

#ifdef __cplusplus
}
#endif
