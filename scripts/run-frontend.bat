@echo off
setlocal

set "ROOT=%~dp0.."
set "FRONTEND_DIR=%ROOT%\frontend"

title StarFlash Rescue Frontend

echo Starting frontend...
echo Directory: %FRONTEND_DIR%
echo.

cd /d "%FRONTEND_DIR%" || (
  echo [ERROR] Failed to enter frontend directory.
  pause
  exit /b 1
)

if not exist node_modules (
  echo Installing frontend dependencies...
  npm install || (
    echo [ERROR] npm install failed.
    pause
    exit /b 1
  )
)

npm run dev

echo.
echo Frontend process exited.
pause
