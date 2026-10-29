@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "SOURCE_DIR=%SCRIPT_DIR%..\xingshan\fbb_ws63-shouzaiduan\fbb_ws63-master\src"
for %%I in ("%SOURCE_DIR%") do set "SOURCE_DIR=%%~fI"

set "NO_PAUSE="
set "BUILD_ARGS="

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="--no-pause" (
    set "NO_PAUSE=1"
    shift
    goto parse_args
)
set BUILD_ARGS=%BUILD_ARGS% %1
shift
goto parse_args

:args_done

set "SHORT_ROOT=D:\jr"
set "SHORT_DIR=%SHORT_ROOT%\v"
set "HISPARK_TOOLS=D:\Program Files\HiSpark Studio 25.12.1\tools\Windows"
set "DEVECO_CMAKE=D:\Program Files\Huawei\DevEco Studio\sdk\default\openharmony\native\build-tools\cmake\bin"
set "RESCUE_DEFS=CONFIG_ENABLE_PRODUCTS_SAMPLE=y,CONFIG_SAMPLE_SUPPORT_SLE_UART=y,CONFIG_SAMPLE_SUPPORT_SLE_UART_SERVER=y,CONFIG_SAMPLE_SUPPORT_NORMAL_TYPE=y,CONFIG_SUPPORT_SLE_PERIPHERAL=y,CONFIG_SLE_UART_BUS=0,CONFIG_UART_TXD_PIN=17,CONFIG_UART_RXD_PIN=18"

call :main
set "RC=%ERRORLEVEL%"
if not defined NO_PAUSE pause
exit /b %RC%

:main
if not exist "%SOURCE_DIR%\build.py" (
    echo ERROR: source project not found: %SOURCE_DIR%
    exit /b 1
)

if /I not "%SHORT_ROOT%"=="D:\jr" (
    echo ERROR: refusing to copy outside D:\jr
    exit /b 1
)

if not exist "%DEVECO_CMAKE%\cmake.exe" (
    echo ERROR: DevEco cmake.exe not found: %DEVECO_CMAKE%\cmake.exe
    exit /b 1
)

python -c "import sys, kconfiglib; print(sys.executable)"
if errorlevel 1 (
    echo ERROR: Python is missing the kconfiglib module required by the WS63 build scripts.
    exit /b 1
)

if not exist "%SHORT_ROOT%" mkdir "%SHORT_ROOT%"

echo Sync WS63 victim project to short path...
echo From: %SOURCE_DIR%
echo To:   %SHORT_DIR%
robocopy "%SOURCE_DIR%" "%SHORT_DIR%" /E /XJ /XD "%SOURCE_DIR%\output" "%SOURCE_DIR%\.git" /XF build.log /R:2 /W:1 /NFL /NDL /NP
set "ROBOCOPY_RC=%ERRORLEVEL%"
if %ROBOCOPY_RC% GEQ 8 (
    echo ERROR: robocopy failed with code %ROBOCOPY_RC%
    exit /b %ROBOCOPY_RC%
)

set "PATH=%DEVECO_CMAKE%;%HISPARK_TOOLS%\ninja;%HISPARK_TOOLS%\cc_riscv32_musl_fp_win\bin;%HISPARK_TOOLS%\cc_riscv32_musl_win\bin;%PATH%"

echo.
echo Building WS63 victim firmware from short path...
echo Project: %SHORT_DIR%
echo.
echo CMake:
where cmake
cmake --version
echo.
echo Ninja:
where ninja
ninja --version
echo.

cd /d "%SHORT_DIR%" || exit /b 1
python build.py -c ws63-liteos-app -def=%RESCUE_DEFS% %BUILD_ARGS%
set "BUILD_RC=%ERRORLEVEL%"

if "%BUILD_RC%"=="0" (
    echo.
    echo Sync firmware output back to original project...
    robocopy "%SHORT_DIR%\output" "%SOURCE_DIR%\output" /E /XJ /R:2 /W:1 /NFL /NDL /NP
    if errorlevel 8 (
        echo ERROR: output sync failed.
        exit /b 8
    )
    echo.
    echo Build success. Firmware output:
    echo %SHORT_DIR%\output\ws63\acore\ws63-liteos-app
    echo Original project output:
    echo %SOURCE_DIR%\output\ws63\acore\ws63-liteos-app
) else (
    echo.
    echo Build failed with code %BUILD_RC%.
)

exit /b %BUILD_RC%
