@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install-plugin.ps1"
exit /b %errorlevel%
