@echo off
REM build_sdcc.bat - STC-B compile (SDCC, two-step build for XINIT address fix)
REM
REM Step 1: Compile all C files, link to get .map
REM Step 2: Extract addresses from .map, update main_app.c
REM Step 3: Recompile main_app.c, relink, generate BIN

setlocal
if "%SDCC_HOME%"=="" set "SDCC_HOME=D:\Program Files\keil\SDCC"
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
REM Step 1: Compile all C files (first pass)
REM ============================================================
echo [1/5] SDCC compile C files (pass 1)
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
REM Step 2: Link (first pass, to get .map with correct addresses)
REM ============================================================
echo [2/5] SDCC link (pass 1, for address extraction)
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
if errorlevel 1 ( echo [FAIL] Link pass 1 & popd & exit /b 1 )
popd

REM ============================================================
REM Step 3: Extract addresses from .map and update main_app.c
REM ============================================================
echo [3/5] Extract addresses from .map and update main_app.c
python "%P%update_xinit_addr.py"
if errorlevel 1 ( echo [FAIL] Address extraction & exit /b 1 )

REM ============================================================
REM Step 4: Recompile main_app.c (with correct addresses)
REM ============================================================
echo [4/5] Recompile main_app.c (pass 2, with correct addresses)
del /q "%OUT%\main_app.rel" "%OUT%\main_app.lst" 2>nul
sdcc -c -mmcs51 --model-large --std-sdcc99 --opt-code-size -I "%SRC%" -I "%INC%" -o "%OUT%/" "%SRC%\main_app.c"
if errorlevel 1 ( echo [FAIL] main_app.c pass 2 & exit /b 1 )

REM ============================================================
REM Step 5: Final link and BIN generation
REM ============================================================
echo [5/5] SDCC final link + BIN
pushd "%OUT%"
del /q "DesktopPet_STC.ihx" "DesktopPet_STC.map" 2>nul
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
if errorlevel 1 ( echo [FAIL] Final link & popd & exit /b 1 )
popd

if exist "%OUT%\DesktopPet_STC.ihx" (
    "%SDCC_BIN%\makebin" -s 65536 "%OUT%\DesktopPet_STC.ihx" "%OUT%\DesktopPet_STC_8k.bin" >nul 2>&1
    echo   IHX: %OUT%\DesktopPet_STC.ihx
    if exist "%OUT%\DesktopPet_STC_8k.bin" echo   BIN: %OUT%\DesktopPet_STC_8k.bin (64KB)
)


REM ============================================================
REM Step 6: Fix UART2 interrupt vector (SDCC places it at wrong address)
REM ============================================================
echo [6/6] Fix UART2 interrupt vector
python "%P%fix_uart2_vector.py"
if errorlevel 1 ( echo [WARN] UART2 vector patch failed )

echo.
echo [OK] Build complete
endlocal
