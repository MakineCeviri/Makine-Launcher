@echo off
echo === MakineAI Core Build ===
echo.

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

cd /d C:\cedra\MakineAI

echo.
echo === Configuring Core Library ===
C:\Qt\Tools\CMake_64\bin\cmake.exe -B build/core-release -S core -G Ninja ^
    -DCMAKE_C_COMPILER=cl ^
    -DCMAKE_CXX_COMPILER=cl ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DVCPKG_TARGET_TRIPLET=x64-windows

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo !!! Configure FAILED !!!
    exit /b 1
)

echo.
echo === Building Core Library ===
C:\Qt\Tools\CMake_64\bin\cmake.exe --build build/core-release --config Release -j8

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo !!! Build FAILED !!!
    exit /b 1
)

echo.
echo === Core Build SUCCESS ===
