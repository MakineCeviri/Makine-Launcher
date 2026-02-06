@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if %ERRORLEVEL% NEQ 0 (
    echo VS 2026 environment failed
    exit /b 1
)

cd /d C:\cedra\MakineAI

echo.
echo ============================================
echo   Configure Core Library
echo ============================================
C:\Qt\Tools\CMake_64\bin\cmake.exe -B build/core-release -S core -G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release -DVCPKG_TARGET_TRIPLET=x64-windows
if %ERRORLEVEL% NEQ 0 (
    echo Core configure FAILED
    exit /b 1
)

echo.
echo ============================================
echo   Build Core Library
echo ============================================
C:\Qt\Tools\CMake_64\bin\cmake.exe --build build/core-release --config Release -j8
if %ERRORLEVEL% NEQ 0 (
    echo Core build FAILED
    exit /b 1
)

echo.
echo [SUCCESS] Core library built
echo.

echo ============================================
echo   Configure QML Application
echo ============================================
C:\Qt\Tools\CMake_64\bin\cmake.exe -B build/qml-release -S qml -G Ninja -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.10.1/msvc2022_64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DMAKINEAI_UI_ONLY=OFF
if %ERRORLEVEL% NEQ 0 (
    echo QML configure FAILED
    exit /b 1
)

echo.
echo ============================================
echo   Build QML Application
echo ============================================
C:\Qt\Tools\CMake_64\bin\cmake.exe --build build/qml-release --config Release -j8
if %ERRORLEVEL% NEQ 0 (
    echo QML build FAILED
    exit /b 1
)

echo.
echo ============================================
echo   BUILD COMPLETE!
echo ============================================
echo.
