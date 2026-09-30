@echo off
rem Usage: tools\run.cmd <executable> [args...]
rem Runs with the Qt kit's bin directory first on PATH (development only; use windeployqt for deployment).
setlocal
set "QTROOT=D:\Qt\Qt-6.11.1-VC2026-x64-D-MD-OCI-2026-05-27"
set "PATH=%QTROOT%\bin;%PATH%"
set "EXE=%~1"
shift
"%EXE%" %1 %2 %3 %4 %5 %6 %7 %8 %9
