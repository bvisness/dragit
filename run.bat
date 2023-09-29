@echo off
setlocal enabledelayedexpansion

call build.bat
if !ERRORLEVEL! neq 0 exit /b !ERRORLEVEL!

build\Dragit\bin\Dragit.exe
