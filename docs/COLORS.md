# Lottie Colors

Four native color pickers replace exact authored RGBs: #f318e8, #9c32fe, #277ff4 and #00f3e9. Changes apply automatically; there is no Use Palette toggle. Other colors, including black/white, remain unchanged.

Shape Fill/Stroke, gradients and layer AE Fill are supported. Animated ordinary Fill/Stroke colors and color expressions are not a reliable palette workflow: prefer fixed RGB with animated opacity. AE Stroke effects and raster pixels are not recolored. Authored transparency is preserved.

Palette preparation runs on source/settings changes rather than rewriting JSON every frame. Emitter captures colors at Spawn. Check the RGB values in the exported JSON: visually similar shades are not palette slots.
