# Lottie for Resolume — Windows beta

By [vnkbln](https://github.com/vnkbln).

Close Resolume. Put LottiePlayer.dll and LottieEmitter.dll in a dedicated folder,
add it in Preferences → Video → FFGL Plugin Directories, then restart Resolume.
Find the plugins in Sources. The LottieLogo demo is embedded.

Requires Windows x64 and OpenGL 4.1. Development host: Arena 7.22.
If the MSVC runtime is missing, install Microsoft's Visual C++ v14 x64 runtime.

Player: select JSON through File. Switch Position from Basic to Timeline or BPM
Sync manually and save a preset. Duration belongs to the host.

Emitter: Spawn starts an instance; Spawn Options controls speed, opacity, instance
limit and activation behavior. Each spawn captures its source, transforms and
palette. Existing instances keep their settings when you change controls/files.

Playlist uses the selected file's folder. Random, Previous/Next, Next on Start
and Emitter Spawn on Change control navigation. One-file navigation restarts it.

Lottie Transform acts before rasterization. Crop defaults off; Original preserves
the authored canvas pixel size. Raster images retain their original resolution.
Colors replace #f318e8, #9c32fe, #277ff4 and #00f3e9; other colors stay unchanged.

JSON only. Not every AE feature is supported. Complex effects and many instances
cost performance; extreme slow spatial motion may expose interpolation limits.
Select the same JSON again to reload resources. Back up private-build presets.

Code: MIT. Artwork: CC BY 4.0, credit vnkbln. Keep LICENSE, LICENSE-ASSETS,
ATTRIBUTION and third_party notices. Commercial use is permitted under these terms.

[Source, full documentation and Centerline pack](https://github.com/vnkbln/lottielume)
