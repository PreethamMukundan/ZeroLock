@echo off
rem Usage: item_import.bat [--refresh] [--show-partial]
rem Rebuilds items.json, then creates or updates the item assets. Needs the editor closed for the second step.
setlocal
set ENGINE=Z:\epicgames\UE_5.8\Engine
set PROJECT=%~dp0..\..\ZeroLock.uproject

"%ENGINE%\Binaries\ThirdParty\Python3\Win64\python.exe" "%~dp0build_items.py" %*
if errorlevel 1 exit /b 1

tasklist /fi "imagename eq UnrealEditor.exe" | find /i "UnrealEditor.exe" >nul
if not errorlevel 1 (
    echo The editor is open. Run this in its Cmd box:
    echo py "%~dp0ue_import_items.py"
    exit /b 0
)

"%ENGINE%\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJECT%" -run=pythonscript -script="%~dp0ue_import_items.py" -unattended -nosplash -nullrhi
