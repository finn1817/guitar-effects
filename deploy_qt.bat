@echo off
setlocal
set APP_EXE=build\GuitarEffectsApp.exe
if not exist "%APP_EXE%" (
  echo ERROR: %APP_EXE% not found. Build first.
  exit /b 1
)
set QT_ROOT=C:\Qt\6.6.0\msvc2019_64
set WINDEPLOYQT=%QT_ROOT%\bin\windeployqt.exe
if not exist "%WINDEPLOYQT%" (
  echo ERROR: windeployqt.exe not found at %WINDEPLOYQT%
  exit /b 1
)
echo Deploying Qt runtime to build folder...
"%WINDEPLOYQT%" --release --no-translations --no-opengl-sw "%APP_EXE%"
echo Done. Launching app...
start "" "%APP_EXE%"
endlocal