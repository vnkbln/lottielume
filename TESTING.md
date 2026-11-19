# Validation

The default build runs 14 Release CTest checks:

| Checks | Scope |
|---|---|
| core, png | metadata, seeking, lifecycle and pixel/PNG validation |
| features | images, fonts, expressions and resource isolation |
| transforms, clipping | transforms, fit and render bounds |
| emitter-state | instance timing, lifecycle and limits |
| palette | color replacement, gradients and Fill slots |
| ffgl, ffgl-expressions, ffgl-cpu | real offscreen WGL rendering and host state restoration |
| emitter-ffgl | schema, independent instances and GPU rendering |
| player-playlist, cpu-playlist, emitter-playlist | navigation, value events and retained sources |

Seven CPU checks run in the Windows workflow. WGL checks need a local OpenGL 4.1 GPU/driver. Packaging checks demo bytes, thumbnails, pack structure and ZIP integrity, recording commit/version/hashes.

Native development testing used Arena 7.22. The user confirmed playback, palette UI, GPU improvements, demo, navigation, Position reset and save/reopen. This is not comprehensive host/driver/export certification.

Before publication, test packaged DLLs in Resolume: discovery, Timeline preset, alpha, repeat activation, audio mapping, snapshots, one/many-file navigation, missing-file replacement, save/reopen, fit/crop and shutdown. Local packaging is not a fresh native-host acceptance run.

Historical development notes are archived locally outside public source. Earlier ASan runs instrumented project code, not all static dependencies; no full memory/leak audit is claimed.
