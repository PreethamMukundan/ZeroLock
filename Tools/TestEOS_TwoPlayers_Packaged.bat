@echo off
rem Launches two PACKAGED ZeroLock instances with EOS on one PC, logged in through the EOS Dev Auth Tool.
rem Package first (see CLAUDE.md), then start ONE Dev Auth Tool on PORT and log in two accounts with the credential names below.
set PORT=8080
set CRED1=Dev1
set CRED2=Dev2
set ROOT=E:\Repo\ZeroLock\ZeroLock
set GAME=%ROOT%\Saved\Packaged\Windows\ZeroLock.exe
set MAP=/MoverExamples/Maps/L_CharacterMovementBasics

if not exist "%GAME%" (
	echo Packaged game not found: %GAME%
	pause
	exit /b 1
)

rem Staging doesn't pack Config/Custom/EOS, so -CustomConfig=EOS finds nothing in the pak. A loose copy next to the
rem packaged game is read from disk instead. It holds EOS credentials: local testing only, never ship this folder.
set EOSINI=%ROOT%\Saved\Packaged\Windows\ZeroLock\Config\Custom\EOS\DefaultEngine.ini
if not exist "%ROOT%\Saved\Packaged\Windows\ZeroLock\Config\Custom\EOS" mkdir "%ROOT%\Saved\Packaged\Windows\ZeroLock\Config\Custom\EOS"
copy /Y "%ROOT%\Config\Custom\EOS\DefaultEngine.ini" "%EOSINI%" >nul

set COMMON=%MAP% -CustomConfig=EOS -AUTH_TYPE=Developer -AUTH_LOGIN=localhost:%PORT% -log -windowed -ResX=1200 -ResY=700 -ExecCmds="t.MaxFPS 60,stat fps,stat unit"

rem -abslog gives each instance its own log (both would otherwise write Saved\Logs\ZeroLock.log).
start "ZeroLock %CRED1% (host)" "%GAME%" %COMMON% -AUTH_PASSWORD=%CRED1% -abslog="%ROOT%\Saved\Packaged\Windows\ZeroLock\Saved\Logs\Host.log" -WinX=0 -WinY=40 -ConsoleX=0 -ConsoleY=760
start "ZeroLock %CRED2% (join)" "%GAME%" %COMMON% -AUTH_PASSWORD=%CRED2% -abslog="%ROOT%\Saved\Packaged\Windows\ZeroLock\Saved\Logs\Join.log" -WinX=1220 -WinY=40 -ConsoleX=1220 -ConsoleY=760
