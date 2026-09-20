@echo off
setlocal
set "CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%CSC%" set "CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe"
if not exist "%CSC%" (
  echo [build] csc.exe not found.
  exit /b 1
)
"%CSC%" /nologo /target:winexe /codepage:65001 /r:System.Windows.Forms.dll /r:System.Drawing.dll /out:"%~dp0pomodoro-stats.exe" "%~dp0PomodoroStats.cs"
if errorlevel 1 (
  echo [build] FAILED
  exit /b 1
)
echo [build] OK: %~dp0pomodoro-stats.exe
