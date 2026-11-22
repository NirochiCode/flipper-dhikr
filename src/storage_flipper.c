#include "include/storage_flipper.h"

#include <furi.h>

#define TAG "DhikrStorage"

static DhikrIoStatus map_error(FS_Error error) {
    if(error == FSE_OK) return DhikrIoOk;
    if(error == FSE_NOT_EXIST) return DhikrIoNotFound;
    return DhikrIoError;
}

static DhikrIoStatus flipper_read(
    void* context,
    const char* path,
    char* buffer,
    size_t capacity,
    size_t* length) {
    Storage* storage = context;
    File* file = storage_file_alloc(storage);
    DhikrIoStatus status = DhikrIoOk;
    *length = 0;

    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        status = map_error(storage_file_get_error(file));
        if(status == DhikrIoOk) status = DhikrIoError;
        if(status == DhikrIoError) {
            FURI_LOG_E(TAG, "open %s: %s", path, storage_file_get_error_desc(file));
        }
    } else {
        size_t read = storage_file_read(file, buffer, capacity);
        if(storage_file_get_error(file) != FSE_OK) {
            FURI_LOG_E(TAG, "read %s: %s", path, storage_file_get_error_desc(file));
            status = DhikrIoError;
        } else {
            *length = read;
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    return status;
}

static DhikrIoStatus
    flipper_write(void* context, const char* path, const char* data, size_t length) {
    Storage* storage = context;
    File* file = storage_file_alloc(storage);
    DhikrIoStatus status = DhikrIoError;

    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        size_t written = storage_file_write(file, data, length);
        bool synced = storage_file_sync(file);
        if(written == length && synced) {
            status = DhikrIoOk;
        } else {
            FURI_LOG_E(TAG, "write %s: %s", path, storage_file_get_error_desc(file));
        }
        if(!storage_file_close(file)) status = DhikrIoError;
    } else {
        FURI_LOG_E(TAG, "create %s: %s", path, storage_file_get_error_desc(file));
    }

    storage_file_free(file);
    return status;
}

static DhikrIoStatus flipper_rename(void* context, const char* from, const char* to) {
    Storage* storage = context;
    FS_Error error = storage_common_rename(storage, from, to);
    if(error != FSE_OK) {
        FURI_LOG_E(TAG, "rename %s -> %s: %s", from, to, storage_error_get_desc(error));
    }
    return map_error(error);
}

static DhikrIoStatus flipper_remove(void* context, const char* path) {
    Storage* storage = context;
    return map_error(storage_common_remove(storage, path));
}

void dhikr_flipper_file_io_init(DhikrFileIo* io, Storage* storage) {
    furi_check(io);
    furi_check(storage);
    io->context = storage;
    io->read = flipper_read;
    io->write = flipper_write;
    io->rename = flipper_rename;
    io->remove = flipper_remove;
}
