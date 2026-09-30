@echo off
rem Usage: tools\run.cmd <executable> [args...]
rem Runs with the Qt kit's bin directory first on PATH (development only; use windeployqt for deployment).
setlocal
set "QTROOT=D:\Qt\Qt-6.11.1-VC2026-x64-D-MD-OCI-2026-05-27"
set "PATH=%QTROOT%\bin;%PATH%"
set "EXE=%~1"
rem %* ignores shift and %1..%9 stop at nine, so collect every remaining argument.
set "ARGS="
:collect
shift
if "%~1"=="" goto run
set ARGS=%ARGS% %1
goto collect
:run
"%EXE%"%ARGS%
