@echo off
REM Initialize MSVC environment (correct x86 BuildTools path)
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || (echo Failed to initialize MSVC environment & exit /b 1)

set "QT_PREFIX=C:\Qt\6.6.0\msvc2019_64"
set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"

echo Configuring with Qt prefix: %QT_PREFIX%
"%CMAKE_EXE%" -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=%QT_PREFIX% -DCMAKE_BUILD_TYPE=Release || (echo CMake configuration failed & exit /b 1)

echo Building target...
"%CMAKE_EXE%" --build build --config Release || (echo Build failed & exit /b 1)

echo Build completed. Executable (if successful) should be in the build directory.
