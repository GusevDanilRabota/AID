<#
.SYNOPSIS
    Запускает markov_processor с очисткой старых выходных данных.
.DESCRIPTION
    Удаляет папки output и result, создаёт их заново, затем запускает программу.
    Все аргументы, переданные скрипту, передаются в markov_processor.
.EXAMPLE
    .\run.ps1 --generate --order 2
    Запустит парсинг и генерацию с биграммами.
#>

param(
    [string[]]$Arguments  # Принимаем все аргументы как массив
)

# Цвета для вывода
$Green = "Green"
$Yellow = "Yellow"
$Red = "Red"

# Пути
$outputDir = ".\output"
$resultDir = ".\result"
$executable = ".\markov_processor.exe"

# Проверяем наличие исполняемого файла
if (-not (Test-Path $executable)) {
    Write-Host "Ошибка: Исполняемый файл не найден: $executable" -ForegroundColor $Red
    Write-Host "Сначала скомпилируйте проект: gcc ... -o markov_processor" -ForegroundColor $Yellow
    exit 1
}

# Удаляем старые папки с данными
if (Test-Path $outputDir) {
    Write-Host "Удаление старой папки output..." -ForegroundColor $Yellow
    Remove-Item -Path $outputDir -Recurse -Force
}
if (Test-Path $resultDir) {
    Write-Host "Удаление старой папки result..." -ForegroundColor $Yellow
    Remove-Item -Path $resultDir -Recurse -Force
}

# Создаём пустые папки заново
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
New-Item -ItemType Directory -Path $resultDir -Force | Out-Null

# Выводим команду запуска
Write-Host "Запуск: $executable $Arguments" -ForegroundColor $Green

# Запускаем программу
& $executable $Arguments

# Проверяем код возврата
if ($LASTEXITCODE -eq 0) {
    Write-Host "Программа завершена успешно." -ForegroundColor $Green
} else {
    Write-Host "Программа завершилась с кодом ошибки: $LASTEXITCODE" -ForegroundColor $Red
}