# Architecture

The shared core loads resources and applies palette, transforms and render bounds. ThorVG is statically linked in separate CPU/GPU configurations.

Player maps host Position to time. Value events request Position reset and update File; selecting Timeline/BPM mode remains a host action.

Emitter retains source versions and captures speed, opacity, transform and palette at spawn. Disconnect clears active/queued work; Connect optionally spawns. Navigation does not replace active sources.

Folder playlists are integrated into both plugins. Loading is on demand without background next-file preloading. The standalone Playlist prototype is retired.

Fit maps the authored canvas to output. Crop constrains content to that fitted boundary; Emitter transforms content inside it. Without crop, render bounds remain limited to actual output. Palette preparation happens on source/settings changes rather than JSON rewriting each frame.

release.json is the version authority. Preserve LT01/LT02 IDs, parameter indices and hidden compatibility parameters when changing the UI.
