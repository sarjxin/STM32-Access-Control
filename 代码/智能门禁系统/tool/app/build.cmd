@echo off
setlocal
set "TEMP=D:\app\android\temp"
set "TMP=%TEMP%"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
if errorlevel 1 (
  echo Build failed. See output\logs\build.log.
  pause
  exit /b 1
)
echo APK: %~dp0output\DoorAccess-v1.0.apk
pause
