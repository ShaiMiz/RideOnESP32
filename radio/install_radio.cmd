@echo off
rem Double-click to install the RideOn model and scripts on the T-Lite's SD card.
rem Uses PlatformIO's Python if it is there, else the Windows "py" launcher or python.
setlocal
set "SCRIPT=%~dp0install_radio.py"
set "PIO_PY=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if exist "%PIO_PY%" (
  "%PIO_PY%" "%SCRIPT%" %*
) else (
  where py >nul 2>nul && (py -3 "%SCRIPT%" %*) || (python "%SCRIPT%" %*)
)
echo.
pause
