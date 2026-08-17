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
set "ASSIMP_CACHE=%ASSIMP_BUILD%\CMakeCache.txt"
set "BUILD_STAMP=%ASSIMP_BUILD%\glimmer-assimp-build.stamp"

if /I "%BUILD_CONFIG%"=="Debug" (
    set "ASSIMP_LIBRARY=%ASSIMP_BUILD%\lib\assimp-vc145-mtd.lib"
    set "ZLIB_LIBRARY=%ASSIMP_BUILD%\contrib\zlib\zlibstaticd.lib"
) else (
    set "ASSIMP_LIBRARY=%ASSIMP_BUILD%\lib\assimp-vc145-mt.lib"
    set "ZLIB_LIBRARY=%ASSIMP_BUILD%\contrib\zlib\zlibstatic.lib"
)

if not exist "%ASSIMP_CONFIG%" goto build
if not exist "%ASSIMP_LIBRARY%" goto build
if not exist "%ZLIB_LIBRARY%" goto build
if not exist "%ASSIMP_CACHE%" goto build
if not exist "%BUILD_STAMP%" goto build

findstr /x /c:"Schema=2" "%BUILD_STAMP%" >nul || goto build
findstr /x /c:"Configuration=%BUILD_CONFIG%" "%BUILD_STAMP%" >nul || goto build
findstr /x /c:"CCache=OFF" "%BUILD_STAMP%" >nul || goto build
findstr /x /c:"ASSIMP_BUILD_USE_CCACHE:BOOL=OFF" "%ASSIMP_CACHE%" >nul || goto build

set "ASSIMP_COMMIT="
for /f "tokens=*" %%I in ('git.exe -C "%ROOT_DIR%\Glimmer\vendor\assimp" rev-parse HEAD 2^>nul') do set "ASSIMP_COMMIT=%%I"
if not defined ASSIMP_COMMIT goto build
findstr /x /c:"AssimpCommit=%ASSIMP_COMMIT%" "%BUILD_STAMP%" >nul || goto build
exit /b 0

:build
echo Assimp %BUILD_CONFIG% artifacts are missing; configuring and building them now.
call "%~dp0Win-BuildAssimp-vs2026.bat" "%BUILD_CONFIG%"
exit /b %ERRORLEVEL%
