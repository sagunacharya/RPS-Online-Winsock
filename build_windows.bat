@echo off
setlocal

where cl >nul 2>nul
if not errorlevel 1 goto :msvc

where g++ >nul 2>nul
if not errorlevel 1 goto :mingw

echo No C++ compiler found.
echo.
echo Option 1: Open a Visual Studio Developer Command Prompt and run this file.
echo Option 2: Install MinGW-w64/MSYS2 and add g++ to PATH.
pause
exit /b 1

:msvc
echo Building with MSVC...
cl /nologo /EHsc /std:c++17 server.cpp ws2_32.lib /Fe:server.exe
if errorlevel 1 goto :error
cl /nologo /EHsc /std:c++17 client.cpp ws2_32.lib /Fe:client.exe
if errorlevel 1 goto :error
del /q server.obj client.obj 2>nul

echo.
echo Build successful: server.exe and client.exe
pause
exit /b 0

:mingw
echo Building with MinGW g++...
g++ server.cpp -std=c++17 -O2 -o server.exe -lws2_32
if errorlevel 1 goto :error
g++ client.cpp -std=c++17 -O2 -o client.exe -lws2_32
if errorlevel 1 goto :error

echo.
echo Build successful: server.exe and client.exe
pause
exit /b 0

:error
echo.
echo Build failed.
pause
exit /b 1
