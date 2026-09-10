@echo off
setlocal

set "ROOT=%~dp0.."
set "SOLUTION=%ROOT%\build\vs2022-x64\qiven-foundation.sln"

where cmake >nul 2>nul
if errorlevel 1 (
    echo [Qiven] CMake was not found in PATH.
    echo Install CMake 3.28 or newer, or make it available in PATH.
    pause
    exit /b 1
)

pushd "%ROOT%"

echo [Qiven] Generating Visual Studio 2022 x64 solution...
cmake --preset vs2022-x64
if errorlevel 1 goto :error

if not exist "%SOLUTION%" (
    echo [Qiven] Generation completed, but the solution was not found:
    echo %SOLUTION%
    goto :error
)

popd

echo [Qiven] Generation complete:
echo %SOLUTION%
exit /b 0

:error
popd
echo.
echo [Qiven] Visual Studio solution generation failed.
pause
exit /b 1
