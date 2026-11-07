@echo off
rem Double-click to read the car receiver over its Wi-Fi hotspot.
rem Uses PlatformIO's Python if it is there, else the Windows "py" launcher or python.
setlocal
set "SCRIPT=%~dp0receiver_check.py"
set "PIO_PY=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if exist "%PIO_PY%" (
  "%PIO_PY%" "%SCRIPT%" %*
) else (
  where py >nul 2>nul && (py -3 "%SCRIPT%" %*) || (python "%SCRIPT%" %*)
)
echo.
pause
