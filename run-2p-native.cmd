@echo off
setlocal

cd /d "%~dp0"

set "GRP_FILE="
if exist "duke3d.grp" set "GRP_FILE=duke3d.grp"
if not defined GRP_FILE if exist "DUKE3D.GRP" set "GRP_FILE=DUKE3D.GRP"

if not defined GRP_FILE (
    echo No se encontro duke3d.grp o DUKE3D.GRP en la carpeta del proyecto.
    exit /b 1
)

set "GAME_EXE="
if exist "eduke32.exe" set "GAME_EXE=eduke32.exe"
if not defined GAME_EXE if exist "eduke32_fix40.exe" set "GAME_EXE=eduke32_fix40.exe"
if not defined GAME_EXE if exist "eduke32_fix39.exe" set "GAME_EXE=eduke32_fix39.exe"
if not defined GAME_EXE if exist "eduke32_fix38.exe" set "GAME_EXE=eduke32_fix38.exe"
if not defined GAME_EXE if exist "eduke32_fix37.exe" set "GAME_EXE=eduke32_fix37.exe"
if not defined GAME_EXE if exist "eduke32_fix36.exe" set "GAME_EXE=eduke32_fix36.exe"
if not defined GAME_EXE if exist "eduke32_fix35.exe" set "GAME_EXE=eduke32_fix35.exe"
if not defined GAME_EXE if exist "eduke32_fix34.exe" set "GAME_EXE=eduke32_fix34.exe"
if not defined GAME_EXE if exist "eduke32_fix33.exe" set "GAME_EXE=eduke32_fix33.exe"
if not defined GAME_EXE if exist "eduke32_fix32.exe" set "GAME_EXE=eduke32_fix32.exe"
if not defined GAME_EXE if exist "eduke32_fix31.exe" set "GAME_EXE=eduke32_fix31.exe"
if not defined GAME_EXE if exist "eduke32_fix30.exe" set "GAME_EXE=eduke32_fix30.exe"
if not defined GAME_EXE if exist "eduke32_fix29.exe" set "GAME_EXE=eduke32_fix29.exe"
if not defined GAME_EXE if exist "eduke32_fix28.exe" set "GAME_EXE=eduke32_fix28.exe"
if not defined GAME_EXE if exist "eduke32_fix27.exe" set "GAME_EXE=eduke32_fix27.exe"
if not defined GAME_EXE if exist "eduke32_fix26.exe" set "GAME_EXE=eduke32_fix26.exe"
if not defined GAME_EXE if exist "eduke32_fix25.exe" set "GAME_EXE=eduke32_fix25.exe"
if not defined GAME_EXE if exist "eduke32_fix24.exe" set "GAME_EXE=eduke32_fix24.exe"
if not defined GAME_EXE if exist "eduke32_fix23.exe" set "GAME_EXE=eduke32_fix23.exe"
if not defined GAME_EXE if exist "eduke32_fix22.exe" set "GAME_EXE=eduke32_fix22.exe"
if not defined GAME_EXE if exist "eduke32_fix21.exe" set "GAME_EXE=eduke32_fix21.exe"
if not defined GAME_EXE if exist "eduke32_fix20.exe" set "GAME_EXE=eduke32_fix20.exe"
if not defined GAME_EXE if exist "eduke32_fix19.exe" set "GAME_EXE=eduke32_fix19.exe"
if not defined GAME_EXE if exist "eduke32_fix18.exe" set "GAME_EXE=eduke32_fix18.exe"
if not defined GAME_EXE if exist "eduke32_fix17.exe" set "GAME_EXE=eduke32_fix17.exe"
if not defined GAME_EXE if exist "eduke32_fix16.exe" set "GAME_EXE=eduke32_fix16.exe"
if not defined GAME_EXE if exist "eduke32_fix15.exe" set "GAME_EXE=eduke32_fix15.exe"
if not defined GAME_EXE if exist "eduke32_fix14.exe" set "GAME_EXE=eduke32_fix14.exe"
if not defined GAME_EXE if exist "eduke32_fix13.exe" set "GAME_EXE=eduke32_fix13.exe"
if not defined GAME_EXE if exist "eduke32_fix12.exe" set "GAME_EXE=eduke32_fix12.exe"
if not defined GAME_EXE if exist "eduke32_fix11.exe" set "GAME_EXE=eduke32_fix11.exe"
if not defined GAME_EXE if exist "eduke32_fix10.exe" set "GAME_EXE=eduke32_fix10.exe"
if not defined GAME_EXE if exist "eduke32_fix9.exe" set "GAME_EXE=eduke32_fix9.exe"
if not defined GAME_EXE if exist "eduke32_fix8.exe" set "GAME_EXE=eduke32_fix8.exe"
if not defined GAME_EXE if exist "eduke32_fix7.exe" set "GAME_EXE=eduke32_fix7.exe"
if not defined GAME_EXE if exist "eduke32_fix6.exe" set "GAME_EXE=eduke32_fix6.exe"
if not defined GAME_EXE if exist "eduke32_fix5.exe" set "GAME_EXE=eduke32_fix5.exe"
if not defined GAME_EXE if exist "eduke32_fix4.exe" set "GAME_EXE=eduke32_fix4.exe"
if not defined GAME_EXE if exist "eduke32_fix3.exe" set "GAME_EXE=eduke32_fix3.exe"
if not defined GAME_EXE if exist "eduke32_fix2.exe" set "GAME_EXE=eduke32_fix2.exe"
if not defined GAME_EXE if exist "eduke32_fix.exe" set "GAME_EXE=eduke32_fix.exe"
if not defined GAME_EXE if exist "eduke32.exe" set "GAME_EXE=eduke32.exe"

if not defined GAME_EXE (
    echo No se encontro eduke32_fix40.exe, eduke32_fix39.exe, eduke32_fix38.exe, eduke32_fix37.exe, eduke32_fix36.exe, eduke32_fix35.exe, eduke32_fix34.exe, eduke32_fix33.exe, eduke32_fix32.exe, eduke32_fix31.exe, eduke32_fix30.exe, eduke32_fix29.exe, eduke32_fix28.exe, eduke32_fix27.exe, eduke32_fix26.exe, eduke32_fix25.exe, eduke32_fix24.exe, eduke32_fix23.exe, eduke32_fix22.exe, eduke32_fix21.exe, eduke32_fix20.exe, eduke32_fix19.exe, eduke32_fix18.exe, eduke32_fix17.exe, eduke32_fix16.exe, eduke32_fix15.exe, eduke32_fix14.exe, eduke32_fix13.exe, eduke32_fix12.exe, eduke32_fix11.exe, eduke32_fix10.exe, eduke32_fix9.exe, eduke32_fix8.exe, eduke32_fix7.exe, eduke32_fix6.exe, eduke32_fix5.exe, eduke32_fix4.exe, eduke32_fix3.exe, eduke32_fix2.exe, eduke32_fix.exe ni eduke32.exe en la carpeta del proyecto.
    exit /b 1
)

echo Lanzando %GAME_EXE%
"%GAME_EXE%" ^
    -usecwd ^
    -gamegrp "%GRP_FILE%" ^
    -mh splitscreen_rpg.def ^
    -mx package/sdk/samples/splitscr_nativepads.con ^
    -q2 ^
    %*

exit /b %ERRORLEVEL%
