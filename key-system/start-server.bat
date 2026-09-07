@echo off
REM ============================================================
REM  TUNG-WARE KEY SYSTEM - LAUNCHER
REM  Double-click to start the key server.
REM ============================================================

REM -- Set your admin token here (CHANGE THIS!) --
set ADMIN_TOKEN=curtis9121
set KEY_SERVER_URL=http://127.0.0.1:3747
set PORT=3747

echo [TUNG-WARE] Starting key system server on port %PORT%...
echo [TUNG-WARE] Admin token loaded from environment.
echo.

cd /d "%~dp0server"
node server.js

pause
