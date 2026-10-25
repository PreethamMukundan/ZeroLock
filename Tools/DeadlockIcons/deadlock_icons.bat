@echo off
rem Usage: deadlock_icons.bat   |   deadlock_icons.bat --all   |   deadlock_icons.bat --download-only
rem Downloads new Deadlock ability/item icons and imports them into /Game/Data/Images and /Game/Data/ShopImages.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deadlock_icons.ps1" %*
