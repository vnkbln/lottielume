# Third-party notices

The Windows plugins statically include the following components. Keep this
directory and the project licenses with binary distributions.

| Component | Pinned version | License / notice |
|---|---|---|
| ThorVG | 1.0.7, `1eef89b3ae16e7a00ccc7d8fadb493cc27f56d26` | ThorVG-LICENSE.txt |
| FFGL SDK | 2.2, `fda8d4a5904eaf09dd97ac0f95c246bd7f9f9a46` | FFGL-LICENSE.txt |
| nlohmann/json | 3.12.0, `55f93686c01528224f448c19128836e7df245f72` | JSON-LICENSE.txt |
| RapidJSON | bundled with ThorVG | RapidJSON-LICENSE.txt |
| JerryScript | bundled with ThorVG | JerryScript-LICENSE.txt, Apache-2.0.txt |
| LodePNG | bundled with ThorVG | LodePNG-LICENSE.txt |
| WebP | bundled with ThorVG | WebP-LICENSE.txt, including patent grant |
| JPEG | bundled with ThorVG | JPEG-NOTICE.txt |
| Lottie cubic Bézier interpolator | bundled with ThorVG | MPL-2.0.txt, sources below |

The ThorVG interpolator includes an MPL-2.0 notice. Its unmodified source is
provided in `sources/tvgLottieInterpolator.cpp` and `.h`. The complete pinned
upstream source is available at
https://github.com/thorvg/thorvg/tree/1eef89b3ae16e7a00ccc7d8fadb493cc27f56d26 .
The source distribution contains our separate Fill effect patch in
`patches/thorvg-fill-effect-slots.patch`; it modifies the model/parser and GPU
Fill shader. The SDK is not modified. See BUILD.md for the reproducible build.
