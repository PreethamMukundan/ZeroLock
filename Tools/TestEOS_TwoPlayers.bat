@echo off
rem Launches two ZeroLock game instances with EOS on one PC, logged in through the EOS Dev Auth Tool.
rem Start ONE Dev Auth Tool on PORT and log in two accounts with the credential names below (case-sensitive).
set PORT=8080
set CRED1=Dev1
set CRED2=Dev2
set ENGINE=Z:\epicgames\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe
set PROJECT=E:\Repo\ZeroLock\ZeroLock\ZeroLock.uproject
set MAP=/MoverExamples/Maps/L_CharacterMovementBasics
rem Two uncooked editor-build games share one CPU/GPU: cap both at 60 fps so neither starves the other, and show frame times
rem (frame hitches over ~100 ms show up as Mover input gaps / interpolation starvation, i.e. choppy movement).
set COMMON=%MAP% -game -CustomConfig=EOS -AUTH_TYPE=Developer -AUTH_LOGIN=localhost:%PORT% -log -windowed -ResX=1200 -ResY=700 -ExecCmds="t.MaxFPS 60,stat fps,stat unit"

start "ZeroLock %CRED1% (host)" "%ENGINE%" "%PROJECT%" %COMMON% -AUTH_PASSWORD=%CRED1% -WinX=0 -WinY=40 -ConsoleX=0 -ConsoleY=760
start "ZeroLock %CRED2% (join)" "%ENGINE%" "%PROJECT%" %COMMON% -AUTH_PASSWORD=%CRED2% -WinX=1220 -WinY=40 -ConsoleX=1220 -ConsoleY=760
