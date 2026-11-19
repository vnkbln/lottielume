# GPU renderer

Distributed DLLs use ThorVG OpenGL 4.1. The engine, codecs and JerryScript are statically linked; the system MSVC runtime is still needed. Cost depends on geometry, effects, output resolution and overlapping instances. There is no universal FPS guarantee.

The diagnostic CPU fallback shares GPU Player's ID and is excluded from release ZIPs. WGL checks validate rendering and host state restoration; native Resolume acceptance remains separate.
