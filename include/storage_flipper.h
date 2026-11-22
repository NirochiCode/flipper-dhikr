/**
 * @file storage_flipper.h
 * @brief DhikrFileIo implementation on top of the Flipper storage service.
 */
#pragma once

#include <storage/storage.h>

#include "include/storage.h"

/** Fill `io` with callbacks that use `storage` (which must outlive `io`). */
void dhikr_flipper_file_io_init(DhikrFileIo* io, Storage* storage);
