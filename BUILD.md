# Windows build

Install VS2022 Desktop development with C++, Windows SDK, VS CMake component (or CMake 3.20+ on PATH), Git and Python 3.12 on PATH.

```powershell
./scripts/build.ps1
```

Creates .tools, installs pinned tools, clones pinned dependencies, verifies/applies the ThorVG patch and builds CPU/GPU libraries plus plugins. Default: all 14 Release checks; requires a real OpenGL 4.1 GPU/driver. Use `-Tests CPU` for seven CPU checks (also used in CI), or `-Tests None` to build only.

## Outputs

- build/Release/LottiePlayer.dll: distributed GPU Player, LT01.
- build/Release/LottieEmitter.dll: distributed GPU Emitter, LT02.
- build/Release/cpu/LottiePlayer.dll: diagnostic CPU fallback, also LT01; never install beside GPU Player.
- build/Release/lottie-render.exe: development renderer.

ThorVG/codecs/JerryScript are static; MSVC runtime remains a deployment prerequisite.

## Pinned sources

- ThorVG 1.0.7: 1eef89b3ae16e7a00ccc7d8fadb493cc27f56d26.
- FFGL 2.2: fda8d4a5904eaf09dd97ac0f95c246bd7f9f9a46.
- nlohmann/json 3.12.0: 55f93686c01528224f448c19128836e7df245f72.

Other dependency modifications are rejected. Our recorded ThorVG patch enables AE Fill slots and corrects GPU Fill alpha. Preserve patch and notices.

## Artwork and packaging

Canonical demo: assets/demo/LottieLogo.json; icons: assets/icons. Generated includes are committed.

```powershell
.tools/Scripts/python.exe scripts/embed-demo.py
.tools/Scripts/python.exe scripts/embed-demo.py --check
.tools/Scripts/python.exe scripts/embed-thumbnails.py
.tools/Scripts/python.exe tests/thumbnails.py
.tools/Scripts/python.exe scripts/validate-pack.py
```

Fixture regeneration additionally needs fonttools 4.68.0; ordinary builds use committed fixtures. Commit tested changes, then run scripts/package-release.py. See [RELEASE.md](RELEASE.md).
