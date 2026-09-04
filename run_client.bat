@echo off
if not exist client.exe (
    echo client.exe not found. Run build_windows.bat first.
    pause
    exit /b 1
)
client.exe
pause
