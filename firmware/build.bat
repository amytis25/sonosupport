@echo off
setlocal enabledelayedexpansion

rem ===================================================================
rem build.bat
rem
rem Description: Windows build/clean driver for the SonoSupport bench
rem              firmware. Wraps the two CMake steps so the board flags
rem              cannot be typed inconsistently, and fails early with a
rem              readable message instead of a CMake stack trace when
rem              PICO_SDK_PATH is missing or wrong.
rem   build.bat           incremental build
rem   build.bat clean     wipe build\ then build
rem   build.bat wipe      wipe build\ and stop
rem Inputs:  optional "clean" or "wipe" argument; PICO_SDK_PATH env var
rem Outputs: build\sonosupport.uf2 (plus .elf/.bin/.hex/.dis/.map)
rem Author:  Amytis
rem Created: 2026-07-30 (rev 2026-08-08: single target, renamed
rem          sonosupport_bench -> sonosupport in the clean rebuild)
rem ===================================================================

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "TARGET=sonosupport"
set "PICO_BOARD_ARG=pico2_w"
set "PICO_PLATFORM_ARG=rp2350-arm-s"

rem ---- fail early: SDK path ------------------------------------------
if "%PICO_SDK_PATH%"=="" (
    echo.
    echo ERROR: PICO_SDK_PATH is not set.
    echo.
    echo   Set it to your Pico SDK checkout, e.g.:
    echo     setx PICO_SDK_PATH "C:\pico\pico-sdk"
    echo   then open a NEW terminal ^(setx does not affect this one^).
    echo.
    exit /b 1
)

if not exist "%PICO_SDK_PATH%\pico_sdk_init.cmake" (
    echo.
    echo ERROR: PICO_SDK_PATH is set to "%PICO_SDK_PATH%"
    echo        but pico_sdk_init.cmake is not there. Wrong directory?
    echo.
    exit /b 1
)

rem ---- SDK >= 2.0 is required for RP2350 -----------------------------
if not exist "%PICO_SDK_PATH%\src\rp2350" (
    echo.
    echo ERROR: this SDK has no src\rp2350 - it predates RP2350 support.
    echo        The Pico 2 W needs SDK 2.0 or newer.
    echo.
    exit /b 1
)

rem ---- clean / wipe --------------------------------------------------
rem Note: "Device or resource busy" here means a shell or Explorer window
rem is sitting inside build\. Close it, do not chase file corruption.
if /i "%~1"=="wipe" goto :do_wipe
if /i "%~1"=="clean" goto :do_wipe
goto :after_wipe

:do_wipe
echo Removing "%BUILD_DIR%" ...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
if exist "%BUILD_DIR%" (
    echo.
    echo ERROR: could not remove "%BUILD_DIR%".
    echo        Something is holding it open - close any terminal or
    echo        Explorer window whose current directory is inside build\.
    echo.
    exit /b 1
)
if /i "%~1"=="wipe" (
    echo Clean complete. Nothing built ^(use "build.bat" to rebuild^).
    exit /b 0
)

:after_wipe

echo Using PICO_SDK_PATH = %PICO_SDK_PATH%
echo Board = %PICO_BOARD_ARG%   Platform = %PICO_PLATFORM_ARG%
echo.

rem ---- configure ------------------------------------------------------
cmake -G Ninja -S "%SCRIPT_DIR%." -B "%BUILD_DIR%" ^
      -DPICO_BOARD=%PICO_BOARD_ARG% ^
      -DPICO_PLATFORM=%PICO_PLATFORM_ARG%
if errorlevel 1 (
    echo.
    echo ERROR: cmake configure failed.
    exit /b 1
)

rem ---- build ----------------------------------------------------------
cmake --build "%BUILD_DIR%"
if errorlevel 1 (
    echo.
    echo ERROR: build failed.
    exit /b 1
)

rem ---- report ---------------------------------------------------------
set "UF2=%BUILD_DIR%\%TARGET%.uf2"
if not exist "%UF2%" (
    echo.
    echo ERROR: build reported success but %UF2% was not produced.
    exit /b 1
)

echo.
echo Build OK.
echo   UF2: %UF2%
echo   Flash: hold BOOTSEL, plug the Pico 2 W in, drag the .uf2 onto the
echo          RP2350 drive ^(or: picotool load "%UF2%" -fx^).
echo.

endlocal
exit /b 0
