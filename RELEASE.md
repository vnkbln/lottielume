# Release preparation

Repository: https://github.com/vnkbln/lottielume.
Version **1.0.0-beta.1**, intended as prerelease. Local scripts do not create a repository, tag or GitHub release.

1. Run scripts/build.ps1 on a real OpenGL 4.1 capable Windows machine.
2. Review public tracked files, documentation and licenses; commit changes.
3. Run `.tools/Scripts/python.exe scripts/package-release.py`.
4. Test packaged DLLs in Resolume. Standalone WGL checks do not replace native acceptance.
5. When ready, push reviewed source, tag its commit and create a GitHub prerelease. Attach ZIPs, checksums and manifest; use generated notes.

Artifacts go to output/release/1.0.0-beta.1: two-DLL plugin ZIP, Centerline ZIP, clean tracked-source ZIP, checksums and manifest. Source excludes Git history, private projects/assets and build/dependency trees.

Code: MIT. Artwork: CC BY 4.0, vnkbln. Keep dependency notices and MPL-covered source in third_party/sources.

scripts/clean-workspace.py previews explicit removable caches. --apply archives small diagnostics/old packages before deletion. User projects, original artwork, installed DLLs, dependencies and release checkpoints are preserved.

## GitHub repository details

Repository slug: **lottielume**. Keep plugin names **Lottie Player** and **Lottie Emitter**.
Suggested About description: **Play, recolor and spawn vector animations in Resolume.**
All public documentation, release notes and pack descriptions are in English.
