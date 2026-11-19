@echo off
python scripts\apply-thorvg-patches.py
if errorlevel 1 exit /b 1
call "%~dp0msvc-env.cmd"
if errorlevel 1 exit /b 1
set PATH=%CD%\.tools\Scripts;%PATH%
if not exist build-thorvg-gl\build.ninja (
 meson setup build-thorvg-gl deps/thorvg --buildtype=release --default-library=static -Dstatic=true -Dengines=cpu,gl -Dloaders=lottie,png,jpg,webp,ttf,otf -Dextra=lottie_exp -Dthreads=false -Dpartial=false
 if errorlevel 1 exit /b 1
)
meson configure build-thorvg-gl -Dengines=cpu,gl -Dloaders=lottie,png,jpg,webp,ttf,otf -Dextra=lottie_exp
if errorlevel 1 exit /b 1
meson compile -C build-thorvg-gl
if errorlevel 1 exit /b 1

