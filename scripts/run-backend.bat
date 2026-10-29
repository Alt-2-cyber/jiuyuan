@echo off
setlocal

set "ROOT=%~dp0.."
set "BACKEND_DIR=%ROOT%\backend"

title StarFlash Rescue Backend

echo Starting backend...
echo Directory: %BACKEND_DIR%
echo.

cd /d "%BACKEND_DIR%" || (
  echo [ERROR] Failed to enter backend directory.
  pause
  exit /b 1
)

set "MYSQL_URL=jdbc:mysql://localhost:3306/starflash_rescue?useSSL=false&serverTimezone=Asia/Shanghai&allowPublicKeyRetrieval=true&createDatabaseIfNotExist=true"
set "MYSQL_USERNAME=root"
set "MYSQL_PASSWORD=root"
set "HUAWEI_IOTDA_DEVICE_IDS=6a30e6ffcbb0cf6bb965ab80_shouzai,6a30e6ffcbb0cf6bb965ab80_jiu_yuan"
set "HUAWEI_IOTDA_SERVICE_ID=environment"
set "HUAWEI_IOTDA_ENABLED=true"
set "HUAWEI_IOTDA_ENDPOINT=https://ff1fe1ecbb.st1.iotda-app.cn-north-4.myhuaweicloud.com"
set "HUAWEI_IOTDA_INSTANCE_ID=ff1fe1ecbb"
set "HUAWEI_PROJECT_ID=878edd31b8614d428d68d1f56ba9acdc"

set "HUAWEI_CREDENTIALS_CSV=%USERPROFILE%\Downloads\credentials.csv"
if exist "%HUAWEI_CREDENTIALS_CSV%" (
  for /f "usebackq skip=1 tokens=1-3 delims=," %%A in ("%HUAWEI_CREDENTIALS_CSV%") do (
    set "HUAWEICLOUD_SDK_AK=%%~B"
    set "HUAWEICLOUD_SDK_SK=%%~C"
    goto :credentials_loaded
  )
)

:credentials_loaded
if not defined HUAWEICLOUD_SDK_AK (
  echo [WARN] Huawei Cloud AK/SK not loaded. Expected: "%HUAWEI_CREDENTIALS_CSV%"
)

mvn spring-boot:run

echo.
echo Backend process exited.
pause
