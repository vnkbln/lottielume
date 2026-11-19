# Lottie Transform

Fit Mode comes first: Fit, Fill, Stretch, Original. Original centers the authored canvas at its pixel size. Position/Anchor range: ±16384px; Rotation: ±180 degrees. Scale is uniform; axis controls are hidden.

Transforms run inside ThorVG before rasterization. Crop to Canvas is last and defaults off. Emitter keeps the fitted canvas boundary fixed and transforms content inside it. Spawn captures these settings. Rendering without Crop remains bounded by the actual output. Vectors retain detail when enlarged; raster images retain their pixel resolution.
