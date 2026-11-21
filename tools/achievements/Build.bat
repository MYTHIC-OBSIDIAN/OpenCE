@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
title Building OpenCE Achievements

rem Builds Achievements.exe with the csc.exe that ships with Windows (.NET Framework 4).
rem The pictures and the sound are not in the repository: put them in an "assets" folder here
rem (see README.md). Every file found there is built in; any that is missing is drawn plainly.

set "CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%CSC%" set "CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe"
if not exist "%CSC%" (
  echo Could not find the .NET Framework 4 C# compiler ^(csc.exe^).
  echo Turn on ".NET Framework 4.8 Advanced Services" in Windows Features, then run this again.
  pause
  exit /b 1
)

rem Put it next to halo.exe: this folder, or one to three folders up.
set "OUT=%~dp0"
if exist "%~dp0..\..\..\halo.exe" set "OUT=%~dp0..\..\..\"
if exist "%~dp0..\..\halo.exe" set "OUT=%~dp0..\..\"
if exist "%~dp0..\halo.exe" set "OUT=%~dp0..\"
if exist "%~dp0halo.exe" set "OUT=%~dp0"

set "RES="
set "ICON="
if exist "assets\app.ico" (
  set "ICON=/win32icon:assets\app.ico"
  set "RES=!RES! /resource:assets\app.ico,app.ico"
) else if exist "..\..\port\windows\opence-icon.ico" (
  set "ICON=/win32icon:..\..\port\windows\opence-icon.ico"
  set "RES=!RES! /resource:..\..\port\windows\opence-icon.ico,app.ico"
)
for %%F in (assets\*.png assets\*.mp3 assets\*.cur) do set "RES=!RES! /resource:%%F,%%~nxF"
for %%F in (assets\icons\*.png) do set "RES=!RES! /resource:%%F,icons.%%~nxF"

"%CSC%" /nologo /target:winexe /optimize+ /platform:anycpu ^
  /out:"%OUT%Achievements.exe" ^
  !ICON! /win32manifest:app.manifest !RES! ^
  /r:System.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll ^
  Achievements.cs Data.cs GameCursor.cs
if errorlevel 1 (
  echo.
  echo Build failed - copy the messages above and send them over.
  pause
  exit /b 1
)
echo.
echo Built: "%OUT%Achievements.exe"
if not exist "%OUT%halo.exe" (
  echo.
  echo NOTE: halo.exe isn't in this folder or up to three folders above it, so Achievements.exe was left
  echo here. Copy it into the game's folder ^(next to halo.exe^): the game starts it from there.
)
pause
