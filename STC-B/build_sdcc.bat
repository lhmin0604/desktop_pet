@echo off
REM build_sdcc.bat - STC-B compile script (SDCC)
REM
REM Usage: build_sdcc.bat
REM
REM Requirements:
REM   - SDCC 4.x (https://sourceforge.net/projects/sdcc/)
REM   - Set environment variable SDCC_HOME pointing to SDCC install dir
REM     Example: set SDCC_HOME=F:\sdcc
REM   - If not set, default to F:\sdcc
REM
REM Output: output\DesktopPet_STC.ihx (Intel HEX, stc-isp can flash directly)

setlocal

if "%SDCC_HOME%"=="" set "SDCC_HOME=F:\sdcc"
set "SDCC_BIN=%SDCC_HOME%\bin"
if not exist "%SDCC_BIN%\sdcc.exe" (
    echo [ERROR] sdcc.exe not found at %SDCC_BIN%
    echo         set SDCC_HOME to your SDCC install dir, e.g.:
    echo             set SDCC_HOME=C:\sdcc
    exit /b 1
)

set "PATH=%SDCC_BIN%;%PATH%"

set "P=%~dp0"
set "INC=%P%inc"
set "SRC=%P%source"
set "OUT=%P%output"
set "LST=%P%list_sdcc"

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%LST%" mkdir "%LST%"

REM Clean old artifacts
del /q "%OUT%\*.rel" 2>nul
del /q "%OUT%\*.rst" 2>nul
del /q "%OUT%\*.sym" 2>nul
del /q "%OUT%\*.map" 2>nul
del /q "%OUT%\*.mem" 2>nul
del /q "%OUT%\*.ihx" 2>nul
del /q "%OUT%\*.hex" 2>nul
del /q "%OUT%\*.bin" 2>nul
del /q "%LST%\*.lst" 2>nul

REM ============================================================
REM Step 1: Assemble startup with sdas8051
REM ============================================================
echo [1/4] Assembling crt0_sdcc.asm
pushd "%SRC%"
"%SDCC_BIN%\sdas8051.exe" -plosgff crt0_sdcc.asm 2>&1
popd
if not exist "%SRC%\crt0_sdcc.rel" (
    echo [FAIL] Assembly failed - crt0_sdcc.rel not created
    exit /b 1
)

REM ============================================================
REM Step 2: SDCC compile all C files (separately, then link)
REM ============================================================
echo [2/4] SDCC compile C files
sdcc -c -mmcs51 --model-large --std-sdcc99 --opt-code-size -I "%INC%" -o "%OUT%/" "%SRC%\main_pawbox_sdcc.c"
if errorlevel 1 ( echo [FAIL] main_pawbox_sdcc.c & exit /b 1 )

sdcc -c -mmcs51 --model-large --std-sdcc99 --opt-code-size -I "%INC%" -o "%OUT%/" "%SRC%\comm_pawbox_sdcc.c"
if errorlevel 1 ( echo [FAIL] comm_pawbox_sdcc.c & exit /b 1 )

sdcc -c -mmcs51 --model-large --std-sdcc99 --opt-code-size -I "%INC%" -o "%OUT%/" "%SRC%\bsp485_sdcc.c"
if errorlevel 1 ( echo [FAIL] bsp485_sdcc.c & exit /b 1 )

REM ============================================================
REM Step 3: Link all .rel files
REM ============================================================
echo [3/4] SDCC link
pushd "%OUT%"
sdcc -mmcs51 --model-large --opt-code-size -o "DesktopPet_STC.ihx" ^
    "%SRC%\crt0_sdcc.rel" ^
    "%OUT%\main_pawbox_sdcc.rel" ^
    "%OUT%\comm_pawbox_sdcc.rel" ^
    "%OUT%\bsp485_sdcc.rel" ^
    -I "%SDCC_HOME%\include" ^
    -L "%SDCC_HOME%\lib\mcs51"
if errorlevel 1 ( echo [FAIL] Link & popd & exit /b 1 )
popd

REM ============================================================
REM Output files
REM ============================================================
echo.
echo [2/3] Output files
dir /b "%OUT%\*.ihx" "%OUT%\*.map" 2>nul

if exist "%OUT%\DesktopPet_STC.ihx" (
    "%SDCC_BIN%\makebin" -s 65536 "%OUT%\DesktopPet_STC.ihx" "%OUT%\DesktopPet_STC.bin" >nul 2>&1
    if exist "%OUT%\DesktopPet_STC.bin" (
        echo   BIN: %OUT%\DesktopPet_STC.bin
    )
    echo   IHX: %OUT%\DesktopPet_STC.ihx
)

echo.
echo [3/3] Build complete
echo.
echo Flash: open stc-isp, select %OUT%\DesktopPet_STC.ihx or .bin

endlocal
