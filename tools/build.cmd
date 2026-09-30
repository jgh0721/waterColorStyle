@echo off
rem Usage: tools\build.cmd [debug|release] [extra "cmake --build" args...]
rem Loads the VS 2026 developer environment (vcvars64) and builds with the cmake/ninja bundled with VS.
rem The VS tools are put first on PATH so that other cmake copies (e.g. Strawberry Perl) are not used.
setlocal

set "ROOT=%~dp0.."
set "PRESET=debug"
if not "%~1"=="" (
    set "PRESET=%~1"
    shift
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSROOT="
for /f "usebackq tokens=*" %%i in (`call "%VSWHERE%" -latest -version "[18.0,19.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if "%VSROOT%"=="" (
    echo [build] Visual Studio 2026 not found.
    exit /b 1
)
echo [build] Visual Studio: %VSROOT%

rem vcvars64 calls vswhere.exe without a path; put the installer folder on PATH so it does not print an error.
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
rem Ninja reads header dependencies from cl /showIncludes lines and expects the English prefix
rem "Note: including file:". A localized cl prints a translated prefix, the dependencies are lost
rem and header edits stop rebuilding their users. Force English compiler output.
set "VSLANG=1033"
set "VSCMAKE=%VSROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake"
set "PATH=%VSCMAKE%\CMake\bin;%VSCMAKE%\Ninja;%PATH%"

pushd "%ROOT%" || exit /b 1
rem FMSTYLE_CONFIGURE_ARGS: extra configure arguments, e.g. -DQTITAN_SOURCE_DIR=...
cmake --preset %PRESET% %FMSTYLE_CONFIGURE_ARGS% || (popd & exit /b 1)
cmake --build --preset %PRESET% %1 %2 %3 %4 %5 %6
set "RC=%ERRORLEVEL%"
popd
exit /b %RC%
