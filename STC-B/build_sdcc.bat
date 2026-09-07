@echo off
REM build_sdcc.bat - STC-B compile (SDCC, main_app + 8 SDCC native drivers + paw_box 485)
REM
REM Output: output\DesktopPet_STC.ihx (Intel HEX) + output\DesktopPet_STC.bin
REM Requires: SDCC at F:\SDCC (or set SDCC_HOME)
REM
REM FIX: SDCC default startup has genXINIT/genXRAMCLEAR bugs when XSEG size
REM has both high and low bytes non-zero (e.g. 0x010E = 270 bytes).
REM Fix: __sdcc_external_startup hook in main_app.c does correct init and
REM returns 1 to skip the buggy default routines. No custom crt0 needed.

setlocal
if "%SDCC_HOME%"=="" set "SDCC_HOME=F:\SDCC"
set "SDCC_BIN=%SDCC_HOME%\bin"
if not exist "%SDCC_BIN%\sdcc.exe" (
    echo [ERROR] sdcc.exe not found at %SDCC_BIN%
    exit /b 1
)

set "PATH=%SDCC_BIN%;%PATH%"

set "P=%~dp0"
set "INC=%P%inc"
set "SRC=%P%source"
set "OUT=%P%output"
if not exist "%OUT%" mkdir "%OUT%"

REM Clean old artifacts
del /q "%OUT%\*.rel" 2>nul
del /q "%OUT%\*.ihx" 2>nul
del /q "%OUT%\*.hex" 2>nul
del /q "%OUT%\*.bin" 2>nul
del /q "%OUT%\*.map" 2>nul
del /q "%OUT%\*.mem" 2>nul
del /q "%OUT%\*.lk" 2>nul
del /q "%OUT%\*.rst" 2>nul
del /q "%OUT%\*.sym" 2>nul
del /q "%OUT%\*.lst" 2>nul

REM ============================================================
REM Step 1: 编译 11 个 C 文件
REM (startup hook is in main_app.c, no custom crt0 needed)
REM ============================================================
echo [1/3] SDCC compile C files
for %%F in (
    main_app
    comm_pawbox_sdcc
    bsp485_sdcc
    sys
    display
    beep
    keys
    adc_drv
    vib
    hall
    expression
) do (
    echo   compiling %%F.c ...
    sdcc -c -mmcs51 --model-large --std-sdcc99 --opt-code-size -I "%SRC%" -I "%INC%" -o "%OUT%/" "%SRC%\%%F.c"
    if errorlevel 1 ( echo [FAIL] %%F.c & exit /b 1 )
)

REM ============================================================
REM Step 2: 链接 (使用 SDCC 默认 crt0, hook 在 main_app.c 里)
REM ============================================================
echo [2/3] SDCC link
pushd "%OUT%"
sdcc -mmcs51 --model-large --opt-code-size -o "DesktopPet_STC.ihx" ^
    "%OUT%\main_app.rel" ^
    "%OUT%\comm_pawbox_sdcc.rel" ^
    "%OUT%\bsp485_sdcc.rel" ^
    "%OUT%\sys.rel" ^
    "%OUT%\display.rel" ^
    "%OUT%\beep.rel" ^
    "%OUT%\keys.rel" ^
    "%OUT%\adc_drv.rel" ^
    "%OUT%\vib.rel" ^
    "%OUT%\hall.rel" ^
    "%OUT%\expression.rel" ^
    -I "%SDCC_HOME%\include" ^
    -L "%SDCC_HOME%\lib\mcs51"
if errorlevel 1 ( echo [FAIL] Link & popd & exit /b 1 )
popd

REM ============================================================
REM 输出
REM ============================================================
echo.
echo [3/3] Build complete
dir /b "%OUT%\*.ihx" "%OUT%\*.map" 2>nul

if exist "%OUT%\DesktopPet_STC.ihx" (
    "%SDCC_BIN%\makebin" -s 65536 "%OUT%\DesktopPet_STC.ihx" "%OUT%\DesktopPet_STC_8k.bin" >nul 2>&1
    echo   IHX: %OUT%\DesktopPet_STC.ihx
    if exist "%OUT%\DesktopPet_STC_8k.bin" echo   BIN: %OUT%\DesktopPet_STC_8k.bin (64KB)
)

endlocal
