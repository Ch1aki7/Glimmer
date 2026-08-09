@echo off
setlocal
powershell.exe -ExecutionPolicy Bypass -File "%~dp0Verify-Windows.ps1" %*
exit /b %ERRORLEVEL%
