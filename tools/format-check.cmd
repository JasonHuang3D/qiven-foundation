@echo off
setlocal

set "ROOT=%~dp0.."

call "%~dp0resolve-toolchain.cmd"
if errorlevel 1 goto :error

where git >nul 2>nul
if errorlevel 1 (
    echo [Qiven] Git was not found in PATH.
    goto :error
)

pushd "%ROOT%"

echo [Qiven] Checking C/C++ formatting...
for /f "delims=" %%F in ('git ls-files -- "*.c" "*.cc" "*.cpp" "*.cxx" "*.h" "*.hh" "*.hpp" "*.hxx" "*.inl" "*.ipp" "*.tpp"') do (
    "%QIVEN_CLANG_FORMAT%" --dry-run --Werror --style=file "%%F"
    if errorlevel 1 goto :error_popd
)

popd
echo [Qiven] Formatting check passed.
exit /b 0

:error_popd
popd

:error
echo.
echo [Qiven] Formatting check failed.
exit /b 1
