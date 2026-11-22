#include "include/storage.h"

#include <stdlib.h>

#include "include/settings_codec.h"

void dhikr_store_init(DhikrStore* store, const DhikrFileIo* io) {
    if(store == NULL) return;
    store->io = io;
    store->last_crc = 0;
    store->has_last_crc = false;
    store->read_only = false;
}

/** Remember the serialized form of `state` as "already on disk". */
static void store_remember(DhikrStore* store, const DhikrState* state, char* buffer) {
    size_t length = dhikr_settings_serialize(state, buffer, DHIKR_SETTINGS_BUFFER_SIZE);
    store->has_last_crc = length > 0;
    store->last_crc = length > 0 ? dhikr_crc32(buffer, length) : 0;
}

DhikrLoadResult dhikr_store_load(DhikrStore* store, DhikrState* out) {
    dhikr_state_init_defaults(out);
    if(store == NULL || store->io == NULL) return DhikrLoadIoError;
    const DhikrFileIo* io = store->io;

    store->has_last_crc = false;
    store->read_only = false;

    char* buffer = malloc(DHIKR_SETTINGS_BUFFER_SIZE);
    if(buffer == NULL) {
        store->read_only = true;
        return DhikrLoadIoError;
    }

    DhikrLoadResult result;
    size_t length = 0;
    DhikrIoStatus status =
        io->read(io->context, DHIKR_SETTINGS_PATH, buffer, DHIKR_SETTINGS_BUFFER_SIZE, &length);

    if(status == DhikrIoOk) {
        DhikrCodecResult codec = dhikr_settings_parse(buffer, length, out);
        if(codec == DhikrCodecOk) {
            store_remember(store, out, buffer);
            // A leftover temp file is stale once the main file is valid.
            io->remove(io->context, DHIKR_SETTINGS_TMP_PATH);
            result = DhikrLoadOk;
        } else {
            // Keep the damaged/newer file for the user instead of overwriting it.
            if(io->rename(io->context, DHIKR_SETTINGS_PATH, DHIKR_SETTINGS_BACKUP_PATH) !=
               DhikrIoOk) {
                // Could not preserve it: do not overwrite it either.
                store->read_only = true;
            }
            result = codec == DhikrCodecUnsupportedVersion ? DhikrLoadUnsupported :
                                                             DhikrLoadCorrupt;
        }
    } else if(status == DhikrIoNotFound) {
        // The app may have been interrupted between writing the temp file and renaming it.
        length = 0;
        result = DhikrLoadMissing;
        if(io->read(io->context, DHIKR_SETTINGS_TMP_PATH, buffer, DHIKR_SETTINGS_BUFFER_SIZE, &length) ==
           DhikrIoOk) {
            if(dhikr_settings_parse(buffer, length, out) == DhikrCodecOk &&
               io->rename(io->context, DHIKR_SETTINGS_TMP_PATH, DHIKR_SETTINGS_PATH) == DhikrIoOk) {
                store_remember(store, out, buffer);
                result = DhikrLoadRecovered;
            } else {
                dhikr_state_init_defaults(out);
                io->remove(io->context, DHIKR_SETTINGS_TMP_PATH);
            }
        }
    } else {
        store->read_only = true;
        result = DhikrLoadIoError;
    }

    free(buffer);
    return result;
}

DhikrSaveResult dhikr_store_save(DhikrStore* store, const DhikrState* state) {
    if(store == NULL || store->io == NULL || store->read_only) return DhikrSaveFailed;
    const DhikrFileIo* io = store->io;

    char* buffer = malloc(DHIKR_SETTINGS_BUFFER_SIZE);
    if(buffer == NULL) return DhikrSaveFailed;

    DhikrSaveResult result = DhikrSaveFailed;
    size_t length = dhikr_settings_serialize(state, buffer, DHIKR_SETTINGS_BUFFER_SIZE);

    do {
        if(length == 0) break;

        uint32_t crc = dhikr_crc32(buffer, length);
        if(store->has_last_crc && crc == store->last_crc) {
            result = DhikrSaveUnchanged;
            break;
        }

        if(io->write(io->context, DHIKR_SETTINGS_TMP_PATH, buffer, length) != DhikrIoOk) {
            io->remove(io->context, DHIKR_SETTINGS_TMP_PATH);
            break;
        }
        if(io->rename(io->context, DHIKR_SETTINGS_TMP_PATH, DHIKR_SETTINGS_PATH) != DhikrIoOk) {
            // The complete temp file stays and is recovered on the next start.
            break;
        }

        store->last_crc = crc;
        store->has_last_crc = true;
        result = DhikrSaveWritten;
    } while(false);

    free(buffer);
    return result;
}
