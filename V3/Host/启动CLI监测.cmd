@echo off
chcp 65001 >nul
cd /d "%~dp0"
python cli_monitor.py
pause
