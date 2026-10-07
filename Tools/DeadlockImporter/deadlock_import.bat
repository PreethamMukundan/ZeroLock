@echo off
rem Usage: deadlock_import.bat lash apollo   |   deadlock_import.bat --list   |   deadlock_import.bat lash --anims all
setlocal
for /f "usebackq delims=" %%B in (`powershell -NoProfile -Command "(Get-Content '%~dp0config.json' -Raw | ConvertFrom-Json).blender"`) do set "BLENDER=%%B"
if not exist "%BLENDER%" (
    echo Blender not found: "%BLENDER%" - set "blender" in config.json
    exit /b 1
)
"%BLENDER%" -b --factory-startup --python-exit-code 1 --python "%~dp0deadlock_import.py" -- %*
