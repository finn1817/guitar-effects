@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
echo MSVC INCLUDE paths after vcvars:
echo %INCLUDE%
echo ---
echo MSVC LIB paths:
echo %LIB%
echo ---
"C:\Program Files\CMake\bin\cmake.exe" -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.6.0\msvc2019_64
ninja -C build
