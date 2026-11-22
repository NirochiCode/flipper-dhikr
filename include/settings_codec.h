/**
 * @file settings_codec.h
 * @brief Versioned, checksummed text format for DhikrState (hardware independent).
 *
 * Format version 3 (v1.0.2):
 *
 *     FlipperDhikr
 *     version=3
 *     active=c1
 *     feedback.counter_vibration=0
 *     feedback.counter_sound=0
 *     feedback.target_vibration=1
 *     feedback.target_sound=1
 *     next_custom_id=2
 *     custom_profiles=custom,c1
 *     profile.subhanallah.count=12
 *     profile.subhanallah.target=33
 *     profile.subhanallah.rounds=4
 *     ...
 *     profile.c1.name=Istighfar
 *     profile.c1.count=0
 *     profile.c1.target=100
 *     profile.c1.rounds=0
 *     crc32=1A2B3C4D
 *
 * Rules:
 *  - Line 1 is the magic string, line 2 the format version.
 *  - The last line is a CRC-32 (IEEE) over every byte before that line.
 *  - Built-in profiles always exist, are always listed first and keep their
 *    defaults if their keys are missing.
 *  - `custom_profiles` lists the custom profile ids in display order. Each
 *    listed profile needs a valid name, count and target.
 *  - Unknown keys and keys of unknown profiles are ignored (forward
 *    compatible within a version). A known key with an invalid value marks
 *    the whole file as corrupt.
 *
 * Versions 1 (v1.0) and 2 (v1.0.1) used one `feedback=<mode>` key
 * (0 Off, 1 Vibration, 2 Sound, 3 Both). It is migrated to the four switches
 * so the old behaviour is kept: Vibration -> both vibration switches on,
 * Sound -> both sound switches on, Both -> all on, Off -> all off.
 *
 * Version 2 differs from 3 only in the feedback keys. Version 1 (v1.0) is
 * also migrated:
 *  - the fixed profile "custom" becomes the first custom profile
 *    (its name, count and target are kept);
 *  - counts that ran past the target (v1.0 allowed this) are folded into
 *    rounds without losing anything: rounds = count / target,
 *    count = count % target.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "include/profiles.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DHIKR_SETTINGS_MAGIC "FlipperDhikr"
/** Current format version written by this app. */
#define DHIKR_SETTINGS_VERSION 3UL
/** Oldest format version that can still be read (and migrated). */
#define DHIKR_SETTINGS_VERSION_MIN 1UL
/** Upper bound for a serialized file; larger files are rejected as corrupt. */
#define DHIKR_SETTINGS_BUFFER_SIZE 4096

typedef enum {
    DhikrCodecOk,
    /** Wrong magic, bad checksum, malformed line or out-of-range value. */
    DhikrCodecCorrupt,
    /** Written by a newer, incompatible version of the app. */
    DhikrCodecUnsupportedVersion,
} DhikrCodecResult;

/**
 * Serialize `state` into `buffer` (NUL terminated) using the current version.
 * @return number of bytes written (without NUL), or 0 if the state is
 *         invalid or the buffer is too small.
 */
size_t dhikr_settings_serialize(const DhikrState* state, char* buffer, size_t capacity);

/**
 * Parse `length` bytes of `data` (any supported version). On success `out`
 * receives the loaded (and, for older versions, migrated) state; on any
 * failure `out` receives the default state.
 * @param[out] source_version format version found in the file (may be NULL;
 *             0 if it could not be determined)
 */
DhikrCodecResult dhikr_settings_parse(
    const char* data,
    size_t length,
    DhikrState* out,
    uint32_t* source_version);

/** CRC-32 (IEEE 802.3, reflected, poly 0xEDB88320). */
uint32_t dhikr_crc32(const void* data, size_t length);

#ifdef __cplusplus
}
#endif
