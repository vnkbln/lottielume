# Centerline authoring standard

## Canvas and framing

1000×1000, square pixels, center 500,500, 2D. This is a coordinate system, not a maximum output resolution: vectors render at Resolume's output size.

With Fill, 16:9 retains the central 562.5 units of height; 9:16 retains the central 562.5 units of width. Their common safe region is approximately x/y 219–781. Keep central Shapes motifs and essential persistent details inside it with margin; main motifs are roughly 400–520 units across.

Echoes and background geometry may leave this region or the canvas. Do not place unique essential details only at an edge. Extreme aspects and mapping setups need separate Fit/Scale/Crop checks.

## Roles and timing

| Folder | Role | Duration | Frames at 60fps |
|---|---|---:|---:|
| Shapes | Central upper-layer motifs, loops/scratching | 4s | 240 |
| One_Shot | Middle-layer Emitter bursts | 2s | 120 |
| BG | Lower-layer background motion | 8s | 480 |

Start at frame 0; the out point is exclusive. At 120 BPM these durations correspond to 8, 4 and 16 beats, but JSON does not provide automatic BPM synchronization. Resolume controls actual timing.

## Palette

Use exact RGB #f318e8, #9c32fe, #277ff4 and #00f3e9. White/black are neutral, outside palette replacement. One recolorable color is often enough; additional colors should distinguish intentional parts. Use transparency rather than a full black background. Black darkens underlying layers under normal alpha compositing.

Prefer fixed RGB with separately animated opacity. Do not rely on animated ordinary Fill/Stroke colors or color expressions for palette binding. Match Color 1–4 across sources for a consistent set. Verify exported RGB values, not just the appearance in AE.

## Motion and geometry

Prefer outlines, small filled accents and generous negative space. One_Shot favors expansion, radial bursts and acceleration. Shapes should remain readable when paused. BG should leave room for the foreground; peak authored opacity is 100%, with overall brightness controlled in the host. Fades may hide wrapping.

Primary strokes typically start at 3–5 canvas units, secondary strokes at 2–3. Scaled echoes change apparent line width. Check at target size. Loops should return to their original state or wrap invisibly; avoid obvious collective back-and-forth motion in backgrounds.

## Export and organization

Prefer ordinary Shape Layers: Stroke, Fill, Path, Ellipse, Rectangle, Transform and Trim Paths. Use Shape Stroke instead of a masked Solid with AE Stroke effect. Layer Fill is supported, but Shape Fill is simpler for new content.

Avoid unnecessary masks, deep precompositions and complex effects. Base geometry does not require rasters, fonts, expressions or state machines. Organize as role/theme/Variant_Name.json with only animations in themed playlist folders. Variants should differ in motion/density/geometry, not just color.

Before inclusion, check native ThorVG and Resolume playback, endpoints, palette, overlaps and landscape/portrait framing. Small JSON size does not guarantee cheap rendering. Very slow spatial motion can expose interpolation precision limits.
