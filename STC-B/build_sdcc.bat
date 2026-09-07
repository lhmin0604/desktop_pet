@echo off
REM build_sdcc.bat - STC-B compile (SDCC, main_app + 8 SDCC native drivers + paw_box 485)
REM
REM Output: output\DesktopPet_STC.ihx (Intel HEX)
REM Requires: SDCC at F:\SDCC (or set SDCC_HOME)

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

REM ============================================================
REM Step 1: crt0 汇编
REM ============================================================
echo [1/4] Assembling crt0_sdcc.asm
pushd "%SRC%"
"%SDCC_BIN%\sdas8051.exe" -plosgff crt0_sdcc.asm
set ERR=%ERRORLEVEL%
popd
if not %ERR%==0 ( echo [FAIL] sdas8051 & exit /b 1 )
if not exist "%SRC%\crt0_sdcc.rel" ( echo [FAIL] crt0.rel missing & exit /b 1 )
move /y "%SRC%\crt0_sdcc.rel" "%OUT%\" >nul

REM ============================================================
REM Step 2: 编译 11 个 C 文件
REM ============================================================
echo [2/4] SDCC compile C files
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
    sdcc -c -mmcs51 --model-large --std-sdcc99 --opt-code-size -I "%INC%" -o "%OUT%/" "%SRC%\%%F.c"
    if errorlevel 1 ( echo [FAIL] %%F.c & exit /b 1 )
)

REM ============================================================
REM Step 3: 链接
REM ============================================================
echo [3/4] SDCC link
pushd "%OUT%"
sdcc -mmcs51 --model-large --opt-code-size -o "DesktopPet_STC.ihx" ^
    "%OUT%\crt0_sdcc.rel" ^
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
echo [4/4] Build complete
dir /b "%OUT%\*.ihx" "%OUT%\*.map" 2>nul

if exist "%OUT%\DesktopPet_STC.ihx" (
    "%SDCC_BIN%\makebin" -s 65536 "%OUT%\DesktopPet_STC.ihx" "%OUT%\DesktopPet_STC.bin" >nul 2>&1
    echo   IHX: %OUT%\DesktopPet_STC.ihx
    if exist "%OUT%\DesktopPet_STC.bin" echo   BIN: %OUT%\DesktopPet_STC.bin
)

endlocal
