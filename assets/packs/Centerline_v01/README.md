# Centerline

130 Lottie animations by [vnkbln](https://github.com/vnkbln).
Version 0.4. [Catalog](CATALOG.md) · [Authoring standard](STANDARD.md).

1000×1000 transparent canvas, 60fps. 13 thematic folders, 10 animations each:

- 40 Shapes, 4s: central motifs for loops/scratching.
- 60 One_Shot, 2s: independent emitter bursts.
- 30 BG, 8s: background motion.

Adjust fit/scale/crop for your output aspect. A square canvas does not guarantee every detail survives every crop. BG has full peak opacity; reduce brightness in your host. Vector quality and performance depend on the player.

Base colors: #f318e8, #9c32fe, #277ff4, #00f3e9. Black and white are intentional neutrals.

**CC BY 4.0**: commercial use/adaptations allowed with attribution and license notice. Suggested credit: **Animations by vnkbln — CC BY 4.0**. Indicate changes if modified. See LICENSE and ATTRIBUTION.md.

## Three-layer starting setup

1. Bottom: Player with BG/Circles/Quiet_Depth.json, 8-second Timeline loop. Start with reduced layer opacity and adjust in Resolume.
2. Middle: Emitter with One_Shot/Lines/Radial_Streaks.json, Spawn mapped to the beat. Start with Speed 1, Opacity 40–60% and Max Instances 4; these are artistic starting values, not measured performance limits.
3. Top: Player with Shapes/Circles/Halo.json, 4-second loop or audio-driven Position.
4. Use Fill for a central portrait/landscape crop, or Fit for the whole square. Begin at Scale 100%, Position 0 and Rotation 0.
5. Match Color 1–4 across sources. Emitter captures changes at the next Spawn.

## Previews

[Full pack preview](Preview/Pack_Preview.mp4) covers all folders in 52 seconds. Preview/Folders contains 13 side-by-side folder reviews. Contact sheets: Shapes_Contact.png, One_Shot_Contact.png and BG_Contact.png in Preview.

See [v0.4 revisions](REVISION_v04.md) for the 23 revised clips and portrait checks. The Layer_Setup example combines Halo, Radial_Streaks and Quiet_Depth; periodic emissions are an example arrangement, not built-in BPM sync.
