@echo off
REM Builds SnakeForge and starts it. Just double-click this file.
REM Uses the raylib copy in the "raylib" folder next to this file.
cd /d "%~dp0"

REM Use C:\mingw64\bin if gcc is not already on the PATH
where gcc >nul 2>nul
if errorlevel 1 set "PATH=C:\mingw64\bin;%PATH%"

gcc main.c -o snakeforge.exe -O2 -Wall -mwindows -I raylib/include -L raylib/lib -lraylibdll
if errorlevel 1 (
    echo.
    echo Build failed. If it says "Permission denied", close the game first and try again.
    pause
    exit /b 1
)
copy /y raylib\lib\raylib.dll . >nul
echo Built snakeforge.exe - starting the game...
start "" snakeforge.exe
