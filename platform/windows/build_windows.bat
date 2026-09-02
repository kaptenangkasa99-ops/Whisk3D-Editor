@echo off
REM ============================================================================
REM  build_windows.bat - compila Whisk3D para Windows (Visual Studio / MSVC).
REM  Nativo: corre en cmd / doble-click, SIN necesitar Git Bash ni WSL.
REM  Requiere: git, cmake y las C++ Build Tools de Visual Studio en el PATH
REM  (ver platform\windows\README.md). Genera platform\windows\build\Release\whisk3d.exe
REM  con la carpeta res\ al lado. Si NSIS esta instalado, ademas arma el instalador
REM  Whisk3D-<version>-win64.exe (si no, lo omite; el .exe suelto ya funciona).
REM
REM  Uso:   platform\windows\build_windows.bat            (Release)
REM         platform\windows\build_windows.bat Debug      (o el config que quieras)
REM ============================================================================
setlocal

REM raiz del repo = 2 niveles arriba de este script (platform\windows\)
set "ROOT=%~dp0..\.."
pushd "%ROOT%" || exit /b 1

where cmake >nul 2>nul || (echo ERROR: cmake is not in PATH. Install it from https://cmake.org/download/ && goto :fail)
where git   >nul 2>nul || (echo ERROR: git is not in PATH. && goto :fail)

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"

echo == Updating submodules (SDL2, Whisk3DCore, WhiskUI)...
git submodule update --init --recursive || goto :fail

echo == Configuring (platform\windows\build)... (cmake detects the installed Visual Studio; x64 by default)
cmake -S . -B platform\windows\build || goto :fail

echo == Building (%CONFIG%)...
cmake --build platform\windows\build --config %CONFIG% --parallel || goto :fail

echo.
echo Whisk3D built:
echo   %ROOT%\platform\windows\build\%CONFIG%\whisk3d.exe
echo.

REM ---- instalador .exe (NSIS, opcional): si NSIS esta, lo genera solo ----
set "PF86=%ProgramFiles(x86)%"
set "PF=%ProgramFiles%"
set "NSISDIR="
where makensis >nul 2>nul && set "NSISDIR=."
if not defined NSISDIR if exist "%PF86%\NSIS\makensis.exe" set "NSISDIR=%PF86%\NSIS"
if not defined NSISDIR if exist "%PF%\NSIS\makensis.exe"   set "NSISDIR=%PF%\NSIS"
if not defined NSISDIR goto :sin_nsis

if not "%NSISDIR%"=="." set "PATH=%PATH%;%NSISDIR%"
echo == Generating installer .exe with NSIS...
pushd platform\windows\build
cpack -G NSIS -C %CONFIG%
set "CPACKERR=%errorlevel%"
popd
if not "%CPACKERR%"=="0" (
  echo *** Installer FAILED ^(cpack^). The whisk3d.exe above was still built successfully. ***
  goto :done
)
echo.
echo Installer generated in:
for %%F in ("%ROOT%\platform\windows\build\Whisk3D-*-win64.exe") do echo   %%~fF
goto :done

:sin_nsis
echo Installer .exe: NSIS not found, so it is SKIPPED ^(the whisk3d.exe above already works^).
echo   To generate it: install NSIS  [winget install NSIS.NSIS]  and run this .bat again.

:done
popd
pause
exit /b 0

:fail
echo.
echo *** Build FAILED. Check the error above. ***
popd
pause
exit /b 1
