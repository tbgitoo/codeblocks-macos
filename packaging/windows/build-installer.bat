@echo off
setlocal EnableExtensions

REM Refresh the IDE stage, then build codeblocks-v3.4-win64.exe with stock NSIS 3.
REM 7zr.exe and the WinLibs archive are not inputs.

cd /d "%~dp0"
call "%~dp0stage.bat"
if errorlevel 1 exit /b 1

set "MAKENSIS="
where makensis >nul 2>&1
if not errorlevel 1 (
    for /f "delims=" %%M in ('where makensis') do (
        if not defined MAKENSIS set "MAKENSIS=%%M"
    )
)
if not defined MAKENSIS if exist "%ProgramFiles(x86)%\NSIS\Bin\makensis.exe" set "MAKENSIS=%ProgramFiles(x86)%\NSIS\Bin\makensis.exe"
if not defined MAKENSIS if exist "%ProgramFiles%\NSIS\Bin\makensis.exe" set "MAKENSIS=%ProgramFiles%\NSIS\Bin\makensis.exe"
if not defined MAKENSIS (
    echo build-installer.bat: makensis.exe was not found. Install stock NSIS 3 and put it on PATH. 1>&2
    exit /b 1
)

"%MAKENSIS%" /V2 "%~dp0codeblocks.nsi"
if errorlevel 1 (
    echo build-installer.bat: makensis failed. 1>&2
    exit /b 1
)
if not exist "%~dp0codeblocks-v3.4-win64.exe" (
    echo build-installer.bat: codeblocks-v3.4-win64.exe was not written. 1>&2
    exit /b 1
)

echo build-installer.bat: codeblocks-v3.4-win64.exe is ready.
exit /b 0
