//
// SPDX-FileCopyrightText: The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#include <gui/Surface.h>

// lib-imsvideocodec.so calls operator new() for this object with its size baked
// in, then only ever uses the ANativeWindow prefix, so the total size is the
// whole ABI it depends on. Repatch the MOVZ immediate in extract-files.py to
// match.
static_assert(sizeof(android::Surface) == 1160,
              "sizeof(Surface) changed; repatch lib-imsvideocodec.so");
