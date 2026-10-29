@echo off
setlocal

set "ROOT=%~dp0"
set "BACKEND_DIR=%ROOT%backend"
set "FRONTEND_DIR=%ROOT%frontend"
set "APP_URL=http://127.0.0.1:5173"

title StarFlash Rescue Launcher

echo.
echo ============================================================
echo   StarFlash Rescue System Launcher
echo ============================================================
echo.

if not exist "%BACKEND_DIR%\pom.xml" (
  echo [ERROR] Backend project not found: "%BACKEND_DIR%"
  pause
  exit /b 1
)

if not exist "%FRONTEND_DIR%\package.json" (
  echo [ERROR] Frontend project not found: "%FRONTEND_DIR%"
  pause
  exit /b 1
)

where java >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Java was not found. Please install JDK 17 or newer.
  pause
  exit /b 1
)

where mvn >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Maven was not found. Please install Maven and add it to PATH.
  pause
  exit /b 1
)

where node >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Node.js was not found. Please install Node.js.
  pause
  exit /b 1
)

where npm >nul 2>nul
if errorlevel 1 (
  echo [ERROR] npm was not found. Please check your Node.js installation.
  pause
  exit /b 1
)

echo [1/3] Starting backend at http://127.0.0.1:8088
start "StarFlash Rescue Backend" "%ROOT%scripts\run-backend.bat"

echo [2/3] Starting frontend at http://127.0.0.1:5173
start "StarFlash Rescue Frontend" "%ROOT%scripts\run-frontend.bat"

echo [3/3] Waiting and opening browser...
timeout /t 10 /nobreak >nul
start "" "%APP_URL%"

echo.
echo Launcher finished.
echo Backend window: StarFlash Rescue Backend
echo Frontend window: StarFlash Rescue Frontend
echo Admin URL: %APP_URL%
echo.
echo Close the backend and frontend command windows to stop services.
echo.
pause
