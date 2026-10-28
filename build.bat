@echo off
setlocal
rem Builds DrDirectory (Release) with MSVC 2022 + Qt 6 and deploys the Qt runtime next to the exe.
if "%QTDIR%"=="" set QTDIR=C:\Qt\6.10.2\msvc2022_64
set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
call "%VCVARS%" >nul || (echo vcvars64.bat not found & exit /b 1)
set PATH=C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%QTDIR%\bin;%PATH%
set SRC=%~dp0.
set OUT=%~dp0build
cmake -S "%SRC%" -B "%OUT%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QTDIR%" || exit /b 1
cmake --build "%OUT%" || exit /b 1
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw "%OUT%\DrDirectory.exe" >nul || exit /b 1
echo.
echo Built: %OUT%\DrDirectory.exe
