@echo off
setlocal

set "ROOT_DIR=%~dp0.."
set "BUILD_CONFIG=%~1"

if "%BUILD_CONFIG%"=="" set "BUILD_CONFIG=Debug"
if /I not "%BUILD_CONFIG%"=="Debug" if /I not "%BUILD_CONFIG%"=="Release" (
    echo Usage: %~nx0 [Debug^|Release]
    exit /b 2
)

set "ASSIMP_BUILD=%ROOT_DIR%\Glimmer\vendor\assimp-build\vs2026-%BUILD_CONFIG%"
set "ASSIMP_CONFIG=%ASSIMP_BUILD%\include\assimp\config.h"

if /I "%BUILD_CONFIG%"=="Debug" (
    set "ASSIMP_LIBRARY=%ASSIMP_BUILD%\lib\assimp-vc145-mtd.lib"
    set "ZLIB_LIBRARY=%ASSIMP_BUILD%\contrib\zlib\zlibstaticd.lib"
) else (
    set "ASSIMP_LIBRARY=%ASSIMP_BUILD%\lib\assimp-vc145-mt.lib"
    set "ZLIB_LIBRARY=%ASSIMP_BUILD%\contrib\zlib\zlibstatic.lib"
)

if exist "%ASSIMP_CONFIG%" if exist "%ASSIMP_LIBRARY%" if exist "%ZLIB_LIBRARY%" exit /b 0

echo Assimp %BUILD_CONFIG% artifacts are missing; configuring and building them now.
call "%~dp0Win-BuildAssimp-vs2026.bat" "%BUILD_CONFIG%"
exit /b %ERRORLEVEL%
