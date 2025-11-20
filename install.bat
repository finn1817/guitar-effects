@echo off
echo ========================================
echo Guitar Effects App - Windows Installer
echo ========================================
echo.

REM Check for admin privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script requires administrator privileges.
    echo Please right-click and select "Run as administrator"
    pause
    exit /b 1
)

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo ERROR: CMakeLists.txt not found!
    echo Please run this script from the project root directory.
    pause
    exit /b 1
)

echo Step 1: Checking for Chocolatey...
where choco >nul 2>&1
if %errorLevel% neq 0 (
    echo Installing Chocolatey package manager...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))"
    if %errorLevel% neq 0 (
        echo ERROR: Failed to install Chocolatey
        pause
        exit /b 1
    )
    refreshenv
)
echo Chocolatey is installed!
echo.

echo Step 2: Installing dependencies...
echo This may take several minutes...
choco install -y cmake git mingw
if %errorLevel% neq 0 (
    echo ERROR: Failed to install dependencies
    pause
    exit /b 1
)
echo Dependencies installed!
echo.

echo Step 3: Downloading Qt6...
if not exist "C:\Qt\6.6.0" (
    echo Downloading Qt6 (this may take a while)...
    mkdir C:\Qt 2>nul
    powershell -Command "& {Invoke-WebRequest -Uri 'https://download.qt.io/official_releases/qt/6.6/6.6.0/single/qt-everywhere-src-6.6.0.zip' -OutFile 'qt6.zip'}"
    powershell -Command "Expand-Archive -Path 'qt6.zip' -DestinationPath 'C:\Qt\' -Force"
    del qt6.zip
)
echo Qt6 ready!
echo.

echo Step 4: Downloading miniaudio...
if not exist "miniaudio" (
    mkdir miniaudio
    cd miniaudio
    powershell -Command "& {Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h' -OutFile 'miniaudio.h'}"
    cd ..
)
echo miniaudio ready!
echo.

echo Step 5: Building the project...
if exist "build" rmdir /s /q build
mkdir build
cd build

cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.6.0\mingw_64" ..
if %errorLevel% neq 0 (
    echo ERROR: CMake configuration failed
    cd ..
    pause
    exit /b 1
)

cmake --build . --config Release
if %errorLevel% neq 0 (
    echo ERROR: Build failed
    cd ..
    pause
    exit /b 1
)

cd ..
echo Build complete!
echo.

echo Step 6: Creating application data directories...
mkdir "%APPDATA%\GuitarEffectsApp\Presets" 2>nul
mkdir "%APPDATA%\GuitarEffectsApp\Clips" 2>nul
echo Directories created!
echo.

echo ========================================
echo Installation complete!
echo ========================================
echo.
echo To run the app, double-click run.bat
echo or execute: .\build\GuitarEffectsApp.exe
echo.
pause