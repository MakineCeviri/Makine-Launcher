@echo off
echo === MakineAI Build Script === > C:\cedra\build.log 2>&1
echo. >> C:\cedra\build.log 2>&1

:: Visual Studio 2022 ortamini yukle
echo Loading VS environment... >> C:\cedra\build.log 2>&1
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >> C:\cedra\build.log 2>&1
if errorlevel 1 (
    echo ERROR: Failed to initialize VS environment >> C:\cedra\build.log 2>&1
    exit /b 1
)

:: QML projesine git
cd /d C:\cedra\MakineAI\qml

:: Ninja ile build et
echo Building with Ninja... >> C:\cedra\build.log 2>&1
C:\Qt\Tools\Ninja\ninja.exe -C build_msvc >> C:\cedra\build.log 2>&1

if errorlevel 1 (
    echo BUILD FAILED! >> C:\cedra\build.log 2>&1
    exit /b 1
)

echo === BUILD SUCCESSFUL === >> C:\cedra\build.log 2>&1
