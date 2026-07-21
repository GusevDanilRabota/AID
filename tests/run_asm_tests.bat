@echo off
cd /d "%~dp0\.."          # переходим из tests в корень проекта
if not exist build mkdir build
cd build
echo ===== Building test_asm =====
cmake --build . --target test_asm
if %errorlevel% neq 0 (
    echo Build failed
    pause
    exit /b 1
)
echo ===== Running ASM tests =====
test_asm.exe
pause