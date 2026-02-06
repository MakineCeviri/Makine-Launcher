@echo off
echo === MakineAI Build Script ===
echo.

:: Visual Studio 2022 ortamini yukle
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo ERROR: Failed to initialize VS environment
    exit /b 1
)

:: QML projesine git
cd /d C:\cedra\MakineAI\qml

:: Ninja ile build et
echo Building with Ninja...
C:\Qt\Tools\Ninja\ninja.exe -C build_msvc

if errorlevel 1 (
    echo.
    echo BUILD FAILED!
    exit /b 1
)

echo.
echo === BUILD SUCCESSFUL ===
echo Executable: C:\cedra\MakineAI\qml\build_msvc\MakineAI.exe
