@echo off
rem Turns the packaged Development build (Saved\Packaged\Windows) into a folder + zip you can send to a friend for an
rem online (EOS) test with telemetry. Package first (CLAUDE.md "Package").
rem
rem The zip contains the EOS client credentials and the telemetry ingest key, and the package contains the Deadlock
rem prototype assets: share it privately with testers only, never publicly.
setlocal
set ROOT=%~dp0..
set PKG=%ROOT%\Saved\Packaged\Windows
set OUT=%ROOT%\Saved\FriendBuild\ZeroLock
set ZIP=%ROOT%\Saved\FriendBuild\ZeroLock_FriendBuild.zip

if not exist "%PKG%\ZeroLock.exe" (
	echo Packaged game not found: %PKG%\ZeroLock.exe
	exit /b 1
)
if not exist "%ROOT%\Config\Custom\EOS\DefaultEngine.ini" (
	echo Missing Config\Custom\EOS\DefaultEngine.ini ^(EOS credentials, shared privately^)
	exit /b 1
)

echo Copying the packaged game...
if exist "%OUT%" rmdir /S /Q "%OUT%"
robocopy "%PKG%" "%OUT%" /E /NFL /NDL /NJH /NJS /NP /XF Manifest_*.txt /XD "%PKG%\ZeroLock\Saved" >nul

rem Staging doesn't pack Config/Custom/EOS, so -CustomConfig=EOS needs a loose copy next to the game.
mkdir "%OUT%\ZeroLock\Config\Custom\EOS" 2>nul
copy /Y "%ROOT%\Config\Custom\EOS\DefaultEngine.ini" "%OUT%\ZeroLock\Config\Custom\EOS\DefaultEngine.ini" >nul

rem Telemetry API address + ingest key (UZL_TelemetrySubsystem reads <ProjectDir>\Telemetry.ini).
if exist "%ROOT%\Config\Telemetry.ini" (
	copy /Y "%ROOT%\Config\Telemetry.ini" "%OUT%\ZeroLock\Telemetry.ini" >nul
) else (
	echo Warning: no Config\Telemetry.ini, the build will only record telemetry to local files.
)

> "%OUT%\PlayOnline.bat" (
	echo @echo off
	echo rem ZeroLock online ^(EOS^). First launch opens the Epic login window; after that the login is remembered.
	echo rem Host: press ` and type  ZL.Online.Host     Join: ZL.Online.Find  then  ZL.Online.Join 0
	echo start "" "%%~dp0ZeroLock.exe" /MoverExamples/Maps/L_CharacterMovementBasics -CustomConfig=EOS -log -ExecCmds="ZL.Online.Login"
)

echo Zipping...
if exist "%ZIP%" del "%ZIP%"
powershell -NoProfile -Command "Compress-Archive -Path '%OUT%\*' -DestinationPath '%ZIP%' -CompressionLevel Optimal"
if errorlevel 1 exit /b 1

echo.
echo Folder: %OUT%
echo Zip:    %ZIP%
endlocal
