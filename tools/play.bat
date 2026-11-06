@echo off
cd /d "%~dp0"
dqix.exe data\dqix.nds data\code 0 1 dqix.sav
if errorlevel 1 pause
