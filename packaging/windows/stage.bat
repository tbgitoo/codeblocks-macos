@echo off
setlocal EnableExtensions

REM Copy src\output33_64 into packaging\windows\stage.
REM Import libraries and leftover swap files stay in output33_64. They are not staged.
REM objdump must be on PATH. It is the packager's MinGW objdump, the way zip is external for update.bat.

set "SRC=%~dp0..\..\src\output33_64"
set "STAGE=%~dp0stage"

where objdump >nul 2>&1
if errorlevel 1 (
    echo stage.bat: objdump was not found on PATH. 1>&2
    exit /b 1
)

if not exist "%SRC%\codeblocks.exe" (
    echo stage.bat: "%SRC%" is not a fresh output33_64. 1>&2
    exit /b 1
)

if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%"
if errorlevel 1 (
    echo stage.bat: could not create "%STAGE%". 1>&2
    exit /b 1
)

robocopy "%SRC%" "%STAGE%" /E /XF *.a *.bak *.prev *.RPT /NFL /NDL /NJH /NJS /nc /ns /np
set "RC=%ERRORLEVEL%"
if %RC% GEQ 8 (
    echo stage.bat: copy failed, robocopy exit %RC%. 1>&2
    exit /b 1
)

set "FAIL=0"
call :require "%STAGE%\codeblocks.exe"
call :require "%STAGE%\codeblocks.dll"
call :require "%STAGE%\cb_console_runner.exe"
call :require "%STAGE%\wxmsw334u_gcc_custom.dll"
call :require "%STAGE%\share\CodeBlocks\plugins\debugger.dll"
if not "%FAIL%"=="0" exit /b 1

for /r "%STAGE%" %%F in (gcc.exe gdb.exe *.a *.bak *.prev *.RPT) do (
    if exist "%%F" (
        echo stage.bat: staged file is not part of the IDE payload: %%F 1>&2
        exit /b 1
    )
)

set "FAIL=0"
call :dump "%STAGE%\codeblocks.exe"
for %%P in ("%STAGE%\share\CodeBlocks\plugins\*.dll") do call :dump "%%~fP"
if not "%FAIL%"=="0" exit /b 1

echo stage.bat: IDE stage is ready.
exit /b 0

:require
if exist "%~1" goto :eof
echo stage.bat: missing "%~1". 1>&2
set "FAIL=1"
goto :eof

:dump
for /f "tokens=3" %%D in ('objdump -p "%~1" ^| findstr /c:"DLL Name:"') do call :need "%%D" "%~1"
goto :eof

:need
set "DLL=%~1"
echo %DLL% | findstr /i /b /c:"api-ms-win-" /c:"ext-ms-" >nul && goto :eof
if exist "%STAGE%\%DLL%" goto :eof
call :isos "%DLL%"
if "%ISOS%"=="1" goto :eof
echo stage.bat: "%~2" imports %DLL%, which is not in the stage root and is not a Windows DLL. 1>&2
set "FAIL=1"
goto :eof

:isos
set "ISOS=0"
for %%O in (
    advapi32.dll bcrypt.dll cfgmgr32.dll combase.dll comctl32.dll comdlg32.dll
    crypt32.dll dwmapi.dll gdi32.dll gdiplus.dll glu32.dll hid.dll imagehlp.dll
    imm32.dll iphlpapi.dll kernel32.dll mpr.dll msimg32.dll msvcrt.dll netapi32.dll
    normaliz.dll ntdll.dll ole32.dll oleacc.dll oleaut32.dll opengl32.dll powrprof.dll
    propsys.dll psapi.dll rpcrt4.dll sechost.dll shcore.dll shell32.dll shfolder.dll
    shlwapi.dll setupapi.dll user32.dll userenv.dll usp10.dll uxtheme.dll version.dll
    winhttp.dll winmm.dll winspool.drv wintrust.dll ws2_32.dll wsock32.dll wtsapi32.dll
) do (
    if /i "%~1"=="%%O" set "ISOS=1"
)
goto :eof