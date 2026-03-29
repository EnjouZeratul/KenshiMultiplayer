@echo off
setlocal
cd /d "%~dp0"

:: Try to find CMake
set CMAKE_EXE=
where cmake >nul 2>&1 && set CMAKE_EXE=cmake
if "%CMAKE_EXE%"=="" (
    if exist "C:\Program Files\CMake\bin\cmake.exe" set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"
)
if "%CMAKE_EXE%"=="" (
    if exist "C:\Program Files (x86)\CMake\bin\cmake.exe" set "CMAKE_EXE=C:\Program Files (x86)\CMake\bin\cmake.exe"
)

if "%CMAKE_EXE%"=="" (
    echo CMake not found. Please install CMake and add to PATH, or run from Visual Studio Developer Command Prompt.
    echo Download: https://cmake.org/download/
    pause
    exit /b 1
)

if not exist build mkdir build
cd build

:: Ninja 需要 cl.exe，需先调用 vcvarsall 设置 MSVC 环境
set "VCVARS="
if exist "C:\Program Files\Microsoft Visual Studio\2026\Community\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2026\Community\VC\Auxiliary\Build\vcvarsall.bat"
if "%VCVARS%"=="" if exist "C:\Program Files\Microsoft Visual Studio\2026\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2026\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if "%VCVARS%"=="" if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if "%VCVARS%"=="" if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if "%VCVARS%"=="" if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
if "%VCVARS%"=="" if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"

set CONFIGURED=0
set "NINJA_EXE=%~dp0ninja-win\ninja.exe"
if exist "%NINJA_EXE%" if not "%VCVARS%"=="" (
    call "%VCVARS%" x64 >nul 2>&1
    "%CMAKE_EXE%" .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%"
    if %errorlevel% equ 0 set CONFIGURED=1
)
if %CONFIGURED% equ 0 (
    if exist CMakeCache.txt (
        findstr /C:"Ninja" CMakeCache.txt >nul 2>&1
        if %errorlevel% equ 0 (
            del /q CMakeCache.txt 2>nul
            rd /s /q CMakeFiles 2>nul
        )
    )
    "%CMAKE_EXE%" .. -G "Visual Studio 18 2026" -A x64
    if %errorlevel% equ 0 set CONFIGURED=1
)
if %CONFIGURED% equ 0 (
    "%CMAKE_EXE%" .. -G "Visual Studio 17 2022" -A x64
    if %errorlevel% equ 0 set CONFIGURED=1
)
if %CONFIGURED% equ 0 (
    "%CMAKE_EXE%" .. -G "Visual Studio 16 2019" -A x64
    if %errorlevel% equ 0 set CONFIGURED=1
)

if %CONFIGURED% equ 0 (
    echo.
    echo CMake 无法找到可用的构建工具。请安装其一：
    echo   - Visual Studio 2026 / 2022 / 2019，勾选「使用 C++ 的桌面开发」
    echo   - 或将 ninja.exe 放入 ninja-win 文件夹
    echo.
    pause
    exit /b 1
)

"%CMAKE_EXE%" --build . --config Release

if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo Build complete. Output copied to KenshiMultiplayer root.
pause
