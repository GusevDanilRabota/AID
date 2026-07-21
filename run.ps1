<#
.SYNOPSIS
    Сборка и запуск markov_processor с очисткой старых данных.
.DESCRIPTION
    Компилирует проект (если необходимо), удаляет папки output и result,
    создаёт их заново, затем запускает программу.
    Если аргументы не указаны, по умолчанию запускается интерактивный режим.
.PARAMETER Arguments
    Аргументы командной строки для markov_processor.
.PARAMETER Rebuild
    Принудительная пересборка проекта.
.EXAMPLE
    .\run.ps1
    Запускает интерактивный режим с очисткой.
.EXAMPLE
    .\run.ps1 --generate --order 2
    Запускает генерацию с биграммами.
.EXAMPLE
    .\run.ps1 --rebuild --interactive
    Пересобирает и запускает интерактивный режим.
#>

param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments,
    [switch]$Rebuild
)

# Цвета для вывода
$Green = "Green"
$Yellow = "Yellow"
$Red = "Red"
$Cyan = "Cyan"

# Пути
$sourceDir = ".\src"
$outputDir = ".\output"
$resultDir = ".\result"
$executable = ".\markov_processor.exe"

# Проверяем наличие исходников
if (-not (Test-Path $sourceDir)) {
    Write-Host "Ошибка: Папка src не найдена." -ForegroundColor $Red
    exit 1
}

# Функция компиляции
function Build-Project {
    Write-Host "Компиляция проекта..." -ForegroundColor $Yellow
    $sources = @(
        "$sourceDir\main.c",
        "$sourceDir\ngram_model.c",
        "$sourceDir\tokenizer.c",
        "$sourceDir\c_parser.c",
        "$sourceDir\md_parser.c",
        "$sourceDir\markov.c",
        "$sourceDir\interactive.c"
    )
    $cmd = "gcc -std=c99 -Wall -Wextra -I $sourceDir -o $executable $($sources -join ' ') -lm"
    Write-Host "Выполняется: $cmd" -ForegroundColor $Cyan
    Invoke-Expression $cmd
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Ошибка компиляции!" -ForegroundColor $Red
        exit $LASTEXITCODE
    }
    Write-Host "Компиляция успешна." -ForegroundColor $Green
}

# Проверяем необходимость компиляции
if ($Rebuild) {
    Write-Host "Принудительная пересборка..." -ForegroundColor $Yellow
    Build-Project
} elseif (-not (Test-Path $executable)) {
    Write-Host "Исполняемый файл не найден. Выполняется сборка..." -ForegroundColor $Yellow
    Build-Project
} else {
    # Проверяем, не изменились ли исходники (по времени модификации)
    $srcTime = (Get-ChildItem $sourceDir -Recurse -Include *.c, *.h | Measure-Object -Maximum LastWriteTime).Maximum
    $exeTime = (Get-Item $executable).LastWriteTime
    if ($srcTime -gt $exeTime) {
        Write-Host "Исходники новее исполняемого файла. Пересборка..." -ForegroundColor $Yellow
        Build-Project
    } else {
        Write-Host "Исполняемый файл актуален." -ForegroundColor $Green
    }
}

# Очистка старых данных
Write-Host "Удаление старых папок output и result..." -ForegroundColor $Yellow
if (Test-Path $outputDir) { Remove-Item -Path $outputDir -Recurse -Force }
if (Test-Path $resultDir) { Remove-Item -Path $resultDir -Recurse -Force }

# Создание папок заново
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
New-Item -ItemType Directory -Path $resultDir -Force | Out-Null

# Формируем аргументы для запуска
$runArgs = @()
if ($Arguments.Count -eq 0) {
    # По умолчанию интерактивный режим с параметрами
    $runArgs = @("--interactive", "--order", "1", "--temp", "0.8")
    Write-Host "Аргументы не указаны. Запуск в интерактивном режиме (--interactive)." -ForegroundColor $Yellow
} else {
    $runArgs = $Arguments
}

# Запуск программы
Write-Host "Запуск: $executable $($runArgs -join ' ')" -ForegroundColor $Cyan
& $executable $runArgs

if ($LASTEXITCODE -eq 0) {
    Write-Host "Программа завершена успешно." -ForegroundColor $Green
} else {
    Write-Host "Программа завершилась с кодом ошибки: $LASTEXITCODE" -ForegroundColor $Red
}