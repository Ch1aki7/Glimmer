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

if not exist "%ASSIMP_SOURCE%\CMakeLists.txt" (
    echo Assimp submodule is missing.
    echo Run: git submodule update --init --recursive
    exit /b 1
)

call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" ^
    -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%

cmake -S "%ASSIMP_SOURCE%" -B "%ASSIMP_BUILD%" -G "NMake Makefiles" ^
    -DCMAKE_BUILD_TYPE=%BUILD_CONFIG% ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DUSE_STATIC_CRT=ON ^
    -DASSIMP_BUILD_TESTS=OFF ^
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF ^
    -DASSIMP_BUILD_SAMPLES=OFF ^
    -DASSIMP_BUILD_DOCS=OFF ^
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

cmake --build "%ASSIMP_BUILD%" --target assimp
exit /b %errorlevel%
