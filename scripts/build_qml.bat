@echo off
echo === MakineAI QML Build ===
echo.

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

cd /d C:\cedra\MakineAI

echo.
echo === Configuring QML Application ===
C:\Qt\Tools\CMake_64\bin\cmake.exe -B build/qml-release -S qml -G Ninja ^
    -DCMAKE_C_COMPILER=cl ^
    -DCMAKE_CXX_COMPILER=cl ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH=C:/Qt/6.10.1/msvc2022_64 ^
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows ^
    -DMAKINEAI_UI_ONLY=OFF

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo !!! QML Configure FAILED !!!
    exit /b 1
)

echo.
echo === Building QML Application ===
C:\Qt\Tools\CMake_64\bin\cmake.exe --build build/qml-release --config Release -j8

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo !!! QML Build FAILED !!!
    exit /b 1
)

echo.
echo === QML Build SUCCESS ===
