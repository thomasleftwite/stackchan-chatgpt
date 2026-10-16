@echo off
setlocal
cd /d "%~dp0server"
call npm.cmd run build
if errorlevel 1 goto failed
cd /d "%~dp0"
if not exist "%USERPROFILE%\.platformio\penv\Scripts\python.exe" goto failed
"%USERPROFILE%\.platformio\penv\Scripts\python.exe" tools\run_gemini_device.py --port COM4 --mode gemini --seconds 580
if errorlevel 1 goto failed
exit /b 0
:failed
echo Gemini device service did not start. See the project verification guide.
pause
exit /b 1
