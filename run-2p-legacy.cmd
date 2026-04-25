@echo off
setlocal

set "PROJECT_DIR=%~dp0"
if "%PROJECT_DIR:~-1%"=="\" set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"

for /f "usebackq delims=" %%I in (`wsl wslpath -a "%PROJECT_DIR%"`) do set "WSL_PROJECT_DIR=%%I"

if not defined WSL_PROJECT_DIR (
    echo No se pudo convertir la ruta del proyecto para WSL.
    exit /b 1
)

wsl bash -lc "cd \"$WSL_PROJECT_DIR\" && ./run-2p-legacy.sh"
exit /b %ERRORLEVEL%
