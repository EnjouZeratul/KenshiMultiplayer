@echo off
echo Kenshi Multiplayer - Uninstall
echo.
echo This will remove the KenshiMultiplayer folder.
echo The game directory is NOT modified - no cleanup needed there.
echo.
pause
cd /d "%~dp0"
cd ..
rmdir /s /q "KenshiMultiplayer" 2>nul
if exist "KenshiMultiplayer" (
    echo Failed to remove folder. Please delete manually.
) else (
    echo Uninstall complete.
)
pause
