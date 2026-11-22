/**
 * @file settings_codec.h
 * @brief Versioned, checksummed text format for DhikrState (hardware independent).
 *
 * Example (version 1):
 *
 *     FlipperDhikr
 *     version=1
 *     active=subhanallah
 *     feedback=1
 *     profile.subhanallah.count=12
 *     profile.subhanallah.target=33
 *     ...
 *     profile.custom.name=Custom Dhikr
 *     crc32=1A2B3C4D
 *
 * Rules:
 *  - Line 1 is the magic string, line 2 the format version.
 *  - The last line is a CRC-32 (IEEE) over every byte before that line.
 *  - Unknown keys and unknown profile ids are ignored (forward compatible
 *    within the same version); a known key with an invalid value marks the
 *    whole file as corrupt.
 *  - Built-in profiles missing from the file keep their defaults.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "include/profiles.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DHIKR_SETTINGS_MAGIC "FlipperDhikr"
#define DHIKR_SETTINGS_VERSION 1UL
/** Upper bound for a serialized file; larger files are rejected as corrupt. */
#define DHIKR_SETTINGS_BUFFER_SIZE 2048

typedef enum {
    DhikrCodecOk,
    /** Wrong magic, bad checksum, malformed line or out-of-range value. */
    DhikrCodecCorrupt,
    /** Written by a newer, incompatible version of the app. */
    DhikrCodecUnsupportedVersion,
} DhikrCodecResult;

/**
 * Serialize `state` into `buffer` (NUL terminated).
 * @return number of bytes written (without NUL), or 0 if the state is
 *         invalid or the buffer is too small.
 */
size_t dhikr_settings_serialize(const DhikrState* state, char* buffer, size_t capacity);

/**
 * Parse `length` bytes of `data`. On success `out` receives the loaded
 * state; on any failure `out` receives the default state.
 */
DhikrCodecResult dhikr_settings_parse(const char* data, size_t length, DhikrState* out);

/** CRC-32 (IEEE 802.3, reflected, poly 0xEDB88320). */
uint32_t dhikr_crc32(const void* data, size_t length);

#ifdef __cplusplus
}
#endif
