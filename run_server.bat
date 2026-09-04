@echo off
if not exist server.exe (
    echo server.exe not found. Run build_windows.bat first.
    pause
    exit /b 1
)
server.exe
pause
