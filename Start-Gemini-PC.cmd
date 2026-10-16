@echo off
setlocal
cd /d "%~dp0server"
call npm.cmd run build
if errorlevel 1 goto failed
node dist/init-private-env.js
if errorlevel 1 goto failed
call npm.cmd run gemini:serve
if errorlevel 1 goto failed
exit /b 0
:failed
echo Gemini PC service did not start. See the project verification guide.
pause
exit /b 1
