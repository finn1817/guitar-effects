@echo off
pushd "%~dp0"
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
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Set-ExecutionPolicy Bypass -Scope Process -Force; [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))"
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
choco install -y cmake git mingw python python312
set CHOCO_EXITCODE=%errorlevel%
if "%CHOCO_EXITCODE%"=="0" goto :deps_ok
if "%CHOCO_EXITCODE%"=="3010" (
    echo One or more packages require a reboot (code 3010). Continuing...
    goto :deps_ok
)
echo ERROR: Failed to install dependencies (exit code %CHOCO_EXITCODE%)
pause
exit /b 1
:deps_ok
echo Dependencies installed!
echo.

REM If Python 3.12 failed (choco exit 1603 earlier) fall back gracefully
if not exist "C:\Python312\python.exe" (
    echo NOTE: Python 3.12 not detected (likely MSI 1603 / pending reboot).
    echo Using existing Python; consider rebooting then rerun for cleaner Qt setup.
)

REM Ensure PATH is refreshed so python/pip are available
call refreshenv
echo.

echo Step 3: Installing Qt6 (prebuilt) via aqtinstall...
if not exist "C:\Qt\6.6.0\mingw_64" (
    REM Prefer Python 3.12 to ensure binary wheels for py7zr deps
    set "PYEXE=C:\Python312\python.exe"
    if not exist "%PYEXE%" set "PYEXE=python"

    echo Using Python interpreter: %PYEXE%
    echo Installing aqtinstall Python package...
    REM Install MSVC build tools (needed if py7zr native deps lack wheels)
    choco install -y visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --passive --locale en-US" >nul 2>&1
    if %errorLevel% neq 0 (
        echo WARNING: MSVC Build Tools install failed or skipped. Continuing...
        set BUILDTOOLS_FAILED=1
    ) else (
        echo MSVC Build Tools install step invoked (may take time to finalize).
    )
    "%PYEXE%" -m pip install --upgrade pip
    "%PYEXE%" -m pip install --upgrade aqtinstall
    if %errorLevel% neq 0 (
        echo ERROR: Failed to install aqtinstall
        pause
        exit /b 1
    )
    REM Verify aqt and py7zr are importable (ensures wheels resolved)
    "%PYEXE%" -c "import aqt, py7zr" 2>nul
    if %errorLevel% neq 0 (
        echo ERROR: aqtinstall dependencies failed to install for the active Python.
        echo Tip: Ensure Python 3.12 is installed (we tried C:\Python312\python.exe)
        echo or install Microsoft C++ Build Tools, then rerun.
        pause
        exit /b 1
    )
    echo Downloading and installing Qt 6.6.0 ^(win64_mingw^) to C:\Qt ...
    "%PYEXE%" -m aqt install-qt windows desktop 6.6.0 win64_mingw -m qtmultimedia -O C:\Qt
    if %errorLevel% neq 0 (
        echo ERROR: Qt install failed via aqtinstall
        pause
        exit /b 1
    )
)
echo Qt6 ready!
echo.

echo Step 4: Downloading miniaudio...
if not exist "miniaudio" (
    mkdir miniaudio
    cd miniaudio
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h' -OutFile 'miniaudio.h'"
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
popd
pause