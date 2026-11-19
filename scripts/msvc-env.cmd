@echo off
if /I "%VSCMD_ARG_TGT_ARCH%"=="x64" exit /b 0
set "LOTTIE_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%LOTTIE_VSWHERE%" (
 echo Visual Studio Installer / vswhere not found. Install VS2022 Desktop development with C++.
 exit /b 1
)
set "LOTTIE_VS_ROOT="
for /f "tokens=*" %%i in ('call "%LOTTIE_VSWHERE%" -latest -products * -version 17 -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "LOTTIE_VS_ROOT=%%i"
if not defined LOTTIE_VS_ROOT (
 echo VS2022 C++ x64 tools not found.
 exit /b 1
)
call "%LOTTIE_VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
exit /b %errorlevel%
