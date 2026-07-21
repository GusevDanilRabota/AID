@echo off
setlocal enabledelayedexpansion
:: Переходим в корень проекта (на два уровня выше папки tests)
cd /d "%~dp0\.."

:: Создаём тестовый корпус из одного простого файла
echo ===== Creating test corpus =====
if not exist "tests\corpus" mkdir tests\corpus
echo Наука это систематическое изучение мира. > "tests\corpus\test.md"
:: Второй файл для большего покрытия
echo Наука не терпит морали. Научное познание объективно. > "tests\corpus\science.md"

:: Готовим выходную папку для результатов теста
if exist "output\test" rmdir /s /q "output\test"
mkdir "output\test"

:: Запускаем model_builder (предполагается, что исполняемый файл уже собран в build/)
echo ===== Running model_builder =====
build\model_builder.exe tests\corpus output\test
if %errorlevel% neq 0 (
    echo FAIL: model_builder returned error code %errorlevel%
    exit /b 1
)

:: Проверяем наличие выходных файлов
if not exist "output\test\model.bin" (
    echo FAIL: model.bin was not created
    exit /b 1
)
if not exist "output\test\vocab.txt" (
    echo FAIL: vocab.txt was not created
    exit /b 1
)

:: Простейшая проверка содержимого vocab.txt: должно быть не меньше 5 строк
for /f %%i in ('type "output\test\vocab.txt" ^| find /c /v ""') do set lines=%%i
if !lines! LSS 5 (
    echo FAIL: vocab.txt has fewer than 5 lines, actual lines = !lines!
    exit /b 1
)

echo PASS: model_builder test completed successfully.