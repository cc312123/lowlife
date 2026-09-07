@echo off
REM ============================================================
REM  TUNG-WARE KEY SYSTEM - ADMIN CLI HELPER
REM  Usage examples are shown below.
REM ============================================================
set ADMIN_TOKEN=curtis9121
set KEY_SERVER_URL=http://127.0.0.1:3747

cd /d "%~dp0server"

echo.
echo  TUNG-WARE Admin CLI
echo  ============================================
echo  Commands:
echo    node admin.js create --days 30 --hwids 1 --tier standard
echo    node admin.js create --days 0  --hwids 1 --tier lifetime
echo    node admin.js revoke --key KEY-XXXX-XXXX-XXXX
echo    node admin.js list
echo  ============================================
echo.

cmd /k
