@echo off
setlocal enabledelayedexpansion

if not exist build mkdir build

set ORCA_DIR=orca
set STDLIB_DIR=%ORCA_DIR%\src\libc-shim

:: compile wasm module
set wasmFlags=--target=wasm32^
       --no-standard-libraries ^
       -mbulk-memory ^
       -g ^
       -D__ORCA__ ^
       -Wl,--no-entry ^
       -Wl,--export-dynamic ^
       -isystem %STDLIB_DIR%\include ^
       -I%ORCA_DIR%\src ^
       -I%ORCA_DIR%\src\ext

clang -MJdorp.json %wasmFlags% -o .\build\module.wasm %ORCA_DIR%\src\orca.c %STDLIB_DIR%\src\*.c src\main.c
if !ERRORLEVEL! neq 0 exit /b !ERRORLEVEL!

orca bundle --orca-dir %ORCA_DIR% --name Dragit --resource-dir data --out-dir build build\module.wasm
