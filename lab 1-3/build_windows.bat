@echo off
setlocal
cd /d "%~dp0"
where cl >nul 2>nul
if errorlevel 1 (
    echo Deschide acest script din "x64 Native Tools Command Prompt for VS 2019/2022".
    echo Sau deschide ReutersFeatures.sln in Visual Studio si apasa Ctrl+F5.
    pause
    exit /b 1
)
if not exist bin mkdir bin
cl /nologo /EHsc /std:c++17 /O2 /W4 /utf-8 src\main.cpp /Fo"bin\main.obj" /Fe"bin\ReutersFeatures.exe"
if errorlevel 1 exit /b 1
bin\ReutersFeatures.exe
if errorlevel 1 exit /b 1
pause
