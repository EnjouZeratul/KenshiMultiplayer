@echo off
cd /d "%~dp0\.."
python scripts/health_check.py
pause
