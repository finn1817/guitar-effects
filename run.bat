@echo off
echo Starting Guitar Effects App...
if exist "build\Release\GuitarEffectsApp.exe" (
    start "" "build\Release\GuitarEffectsApp.exe"
) else if exist "build\GuitarEffectsApp.exe" (
    start "" "build\GuitarEffectsApp.exe"
) else (
    echo ERROR: Application not found!
    echo Please run install.bat first.
    pause
)