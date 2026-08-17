@echo off
setlocal

set "ROOT_DIR=%~dp0.."
set "ASSIMP_SOURCE=%ROOT_DIR%\Glimmer\vendor\assimp"
set "BUILD_CONFIG=%~1"

if "%BUILD_CONFIG%"=="" set "BUILD_CONFIG=Debug"
if /I not "%BUILD_CONFIG%"=="Debug" if /I not "%BUILD_CONFIG%"=="Release" (
    echo Usage: %~nx0 [Debug^|Release]
    exit /b 2
)
set "ASSIMP_BUILD=%ROOT_DIR%\Glimmer\vendor\assimp-build\vs2026-%BUILD_CONFIG%"
set "BUILD_STAMP=%ASSIMP_BUILD%\glimmer-assimp-build.stamp"

if not exist "%ASSIMP_SOURCE%\CMakeLists.txt" (
    echo Assimp submodule is missing.
    echo Run: git submodule update --init --recursive
    exit /b 1
)

set "VS_INSTALL="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
    for /f "tokens=*" %%I in ('vswhere.exe -latest -version "[18.0,19.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "VS_INSTALL=%%I"
)
if not defined VS_INSTALL if defined VSINSTALLDIR set "VS_INSTALL=%VSINSTALLDIR%"

if not defined VS_INSTALL (
    echo Visual Studio with the x64 C++ toolchain was not found.
    echo Install the Visual Studio 2026 Desktop development with C++ workload.
    exit /b 1
)

set "VS_DEVCMD=%VS_INSTALL%\Common7\Tools\VsDevCmd.bat"
if not exist "%VS_DEVCMD%" (
    echo Visual Studio developer command script was not found:
    echo %VS_DEVCMD%
    exit /b 1
)

call "%VS_DEVCMD%" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if /I not "%VCToolsVersion:~0,4%"=="14.5" (
    echo Visual Studio 2026 v145 is required, but the active toolset is %VCToolsVersion%.
    exit /b 1
)

set "CMAKE_EXE=%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE_EXE%" (
    where cmake.exe >nul 2>nul
    if errorlevel 1 (
        echo CMake was not found in PATH or the Visual Studio installation.
        exit /b 1
    )
    set "CMAKE_EXE=cmake.exe"
)

where nmake.exe >nul 2>nul
if errorlevel 1 (
    echo NMake was not found after entering the Visual Studio developer environment.
    exit /b 1
)

"%CMAKE_EXE%" -S "%ASSIMP_SOURCE%" -B "%ASSIMP_BUILD%" -G "NMake Makefiles" ^
    -DCMAKE_BUILD_TYPE=%BUILD_CONFIG% ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DUSE_STATIC_CRT=ON ^
    -DASSIMP_BUILD_TESTS=OFF ^
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF ^
    -DASSIMP_BUILD_SAMPLES=OFF ^
    -DASSIMP_BUILD_DOCS=OFF ^
    -DASSIMP_BUILD_USE_CCACHE=OFF ^
    -DASSIMP_NO_EXPORT=ON ^
    -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF ^
    -DASSIMP_BUILD_OBJ_IMPORTER=ON ^
    -DASSIMP_BUILD_FBX_IMPORTER=ON ^
    -DASSIMP_BUILD_GLTF_IMPORTER=ON ^
    -DASSIMP_BUILD_ZLIB=ON ^
    -DASSIMP_INSTALL=OFF ^
    -DASSIMP_WARNINGS_AS_ERRORS=OFF ^
    -DASSIMP_IGNORE_GIT_HASH=ON
if errorlevel 1 exit /b %errorlevel%

"%CMAKE_EXE%" --build "%ASSIMP_BUILD%" --target assimp
if errorlevel 1 exit /b %errorlevel%

set "ASSIMP_COMMIT="
for /f "tokens=*" %%I in ('git.exe -C "%ASSIMP_SOURCE%" rev-parse HEAD 2^>nul') do set "ASSIMP_COMMIT=%%I"
if not defined ASSIMP_COMMIT (
    echo Could not determine the Assimp submodule revision.
    exit /b 1
)

> "%BUILD_STAMP%" (
    echo Schema=2
    echo Configuration=%BUILD_CONFIG%
    echo AssimpCommit=%ASSIMP_COMMIT%
    echo Toolset=%VCToolsVersion%
    echo CCache=OFF
)
exit /b 0
