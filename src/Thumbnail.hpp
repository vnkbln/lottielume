#pragma once
#include <FFGLSDK.h>

#ifdef LOTTIE_EMITTER_THUMBNAIL
#include "ThumbnailEmitter.inc"
#else
#include "ThumbnailPlayer.inc"
#endif

// Register once when the DLL loads; no GL context or external image file is needed.
static CFFGLThumbnailInfo thumbnailInfo(160, 120, [] {
    std::vector<CFFGLColor> colors;
    colors.reserve(160 * 120);
    for (auto rgba : thumbnailPixels)
        colors.emplace_back(rgba);
    return colors;
}());
