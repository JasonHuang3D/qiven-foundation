@echo off
setlocal

set "ROOT=%~dp0.."
set "SOLUTION=%ROOT%\build\vs2022-x64\qiven-foundation.sln"

call "%~dp0resolve-toolchain.cmd"
if errorlevel 1 goto :error

pushd "%ROOT%"

echo [Qiven] Generating Visual Studio 2022 x64 solution...
"%QIVEN_CMAKE%" --preset vs2022-x64
if errorlevel 1 goto :error_popd

if not exist "%SOLUTION%" (
    echo [Qiven] Generation completed, but the solution was not found:
    echo %SOLUTION%
    goto :error_popd
)

popd

echo [Qiven] Generation complete:
echo %SOLUTION%
exit /b 0

:error_popd
popd

:error
echo.
echo [Qiven] Visual Studio solution generation failed.
exit /b 1
