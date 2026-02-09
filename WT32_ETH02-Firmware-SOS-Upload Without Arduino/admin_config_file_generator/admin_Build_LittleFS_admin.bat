@echo off
title ESP32 LittleFS Builder
color 0A

echo ==============================================
echo      ESP32 / ESP32C3 LittleFS BIN Builder
echo ==============================================
echo.

:: ======== USER CONFIGURABLE PATH (edit if needed) ========
set "PROJECT_PATH=D:\Aurdino Examples\WT32_ETH02_SDK_Firmware_update_mqtt-sos-receiver"
set "OUTPUT_BIN=D:\Aurdino Examples\Firmware-SOS-Upload Without Arduino\WT32_ETH02_config.bin"
set "PART_SIZE=1441792"   :: 128KB = 0x20000
set "PAGE_SIZE=256" 
set "BLOCK_SIZE=4096"
:: ==========================================================

:: Ask if user wants to change path
set /p "USERPATH=Enter Sketch Path (Press Enter to use default): "
if not "%USERPATH%"=="" set "PROJECT_PATH=%USERPATH%"

:: Data folder path
set "DATA_PATH=%PROJECT_PATH%\data"

:: Find mklittlefs.exe
set "MKLITTLEFS_PATH=%USERPROFILE%\AppData\Local\Arduino15\packages\esp32\tools\mklittlefs"
for /f "delims=" %%A in ('dir "%MKLITTLEFS_PATH%" /b /s /a-d ^| findstr /i "mklittlefs.exe"') do set "MKLITTLEFS_EXE=%%A"

if not exist "%MKLITTLEFS_EXE%" (
    color 0C
    echo [ERROR] mklittlefs.exe not found!
    echo Please check your Arduino ESP32 core installation.
    pause
    exit /b
)

if not exist "%DATA_PATH%" (
    color 0C
    echo [ERROR] Data folder not found!
    echo Expected at: %DATA_PATH%
    pause
    exit /b
)

echo.
echo Using mklittlefs: %MKLITTLEFS_EXE%
echo Data folder: %DATA_PATH%
echo Output file: %OUTPUT_BIN%
echo.

:: ======== Build Command ========
"%MKLITTLEFS_EXE%" -c "%DATA_PATH%" -p %PAGE_SIZE% -b %BLOCK_SIZE% -s %PART_SIZE% "%OUTPUT_BIN%"

if %ERRORLEVEL% neq 0 (
    color 0C
    echo.
    echo [FAILED] LittleFS build failed!
    pause
    exit /b
)

color 0A
echo.
echo [SUCCESS] LittleFS binary created successfully!
echo File: %OUTPUT_BIN%
echo.

pause
exit /b
