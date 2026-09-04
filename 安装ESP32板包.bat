@echo off
chcp 65001 >nul 2>&1
echo.
echo ==========================================
echo   ESP32 Arduino 板包离线安装器
echo   (从乐鑫中国镜像下载，解决GitHub超时)
echo ==========================================
echo.

python install_esp32.py

if errorlevel 1 (
    echo.
    echo [提示] Python 未安装或不在 PATH 中
    echo 请尝试: python3 install_esp32.py
    echo.
    pause
)
