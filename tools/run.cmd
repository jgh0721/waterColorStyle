@echo off
rem Usage: tools\run.cmd <executable> [args...]
rem Runs with the Qt kit's bin directory first on PATH (development only; use windeployqt for deployment).
setlocal
set "QTROOT=D:\Qt\Qt-6.11.1-VC2026-x64-D-MD-OCI-2026-05-27"
set "PATH=%QTROOT%\bin;%PATH%"
set "EXE=%~1"
rem shift/%%1 split arguments on commas, semicolons and '=' (--only main,copy would lose ",copy"),
rem so take %%* as typed and drop the first token (the executable).
set "ALL=%*"
setlocal EnableDelayedExpansion
set "ARGS=!ALL:*%1=!"
"%EXE%"!ARGS!
