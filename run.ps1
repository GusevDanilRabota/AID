<#
.SYNOPSIS
    Сборка, тестирование и запуск markov_processor.
.DESCRIPTION
    Автоматически собирает тесты, запускает их, затем компилирует и запускает основную программу.
.PARAMETER Arguments
    Аргументы командной строки для markov_processor.
.PARAMETER Rebuild
    Принудительная пересборка всех компонентов.
#>

param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments,
    [switch]$Rebuild
)

# Цвета
$Green = "Green"
$Yellow = "Yellow"
$Red = "Red"
$Cyan = "Cyan"

# Пути
$srcDir = ".\src"
$testDir = ".\tests"
$outputDir = ".\output"
$resultDir = ".\result"
$executable = ".\markov_processor.exe"

# Функция компиляции теста
function Build-Test {
    param($TestName, $Sources)
    $outExe = "$TestName.exe"
    $cmd = "gcc -std=c99 -Wall -Wextra -I $srcDir -o $outExe $Sources -lm"
    Write-Host "Сборка $TestName..." -ForegroundColor $Yellow
    Invoke-Expression $cmd
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Ошибка сборки теста $TestName" -ForegroundColor $Red
        exit $LASTEXITCODE
    }
    return $outExe
}

# Функция запуска теста (исправлена)
function Run-Test {
    param($TestExe)
    Write-Host "Запуск $TestExe..." -ForegroundColor $Cyan
    & ".\$TestExe"   # добавляем ".\" для выполнения из текущей папки
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Тест $TestExe завершился с ошибкой (код $LASTEXITCODE)" -ForegroundColor $Red
        exit $LASTEXITCODE
    } else {
        Write-Host "$TestExe успешно пройден." -ForegroundColor $Green
    }
}

# Функция компиляции основной программы
function Build-Main {
    Write-Host "Компиляция основной программы..." -ForegroundColor $Yellow
    $sources = @(
        "$srcDir\main.c",
        "$srcDir\ngram_model.c",
        "$srcDir\tokenizer.c",
        "$srcDir\c_parser.c",
        "$srcDir\md_parser.c",
        "$srcDir\markov.c",
        "$srcDir\interactive.c",
        "$srcDir\utils.c"
    )
    $cmd = "gcc -std=c99 -Wall -Wextra -I $srcDir -o $executable $($sources -join ' ') -lm"
    Write-Host "Выполняется: $cmd" -ForegroundColor $Cyan
    Invoke-Expression $cmd
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Ошибка компиляции основной программы!" -ForegroundColor $Red
        exit $LASTEXITCODE
    }
    Write-Host "Основная программа собрана успешно." -ForegroundColor $Green
}

# ---- 1. Сборка и запуск тестов ----
Write-Host "=== СБОРКА И ЗАПУСК ТЕСТОВ ===" -ForegroundColor $Yellow

$tests = @(
    @{Name="test_ngram"; Sources=@("$testDir\test_ngram.c", "$srcDir\ngram_model.c")},
    @{Name="test_tokenizer"; Sources=@("$testDir\test_tokenizer.c", "$srcDir\tokenizer.c")},
    @{Name="test_c_parser"; Sources=@("$testDir\test_c_parser.c", "$srcDir\c_parser.c", "$srcDir\tokenizer.c", "$srcDir\ngram_model.c", "$srcDir\utils.c")},
    @{Name="test_md_parser"; Sources=@("$testDir\test_md_parser.c", "$srcDir\md_parser.c", "$srcDir\ngram_model.c", "$srcDir\utils.c")},
    @{Name="test_markov"; Sources=@("$testDir\test_markov.c", "$srcDir\markov.c", "$srcDir\ngram_model.c", "$srcDir\utils.c")},
    @{Name="test_interactive"; Sources=@("$testDir\test_interactive.c", "$srcDir\interactive.c", "$srcDir\markov.c", "$srcDir\ngram_model.c", "$srcDir\utils.c")}
)

foreach ($test in $tests) {
    $exe = "$($test.Name).exe"
    $needRebuild = $Rebuild
    if (-not $needRebuild) {
        $srcTime = (Get-ChildItem $test.Sources -ErrorAction SilentlyContinue | Measure-Object -Maximum LastWriteTime).Maximum
        if ($srcTime -and (Test-Path $exe)) {
            $exeTime = (Get-Item $exe).LastWriteTime
            if ($srcTime -gt $exeTime) { $needRebuild = $true }
        } elseif (-not (Test-Path $exe)) {
            $needRebuild = $true
        }
    }
    if ($needRebuild) {
        $built = Build-Test -TestName $test.Name -Sources ($test.Sources -join ' ')
    } else {
        $built = $exe
    }
    Run-Test $built
}

# ---- 2. Сборка основной программы ----
Write-Host "`n=== СБОРКА ОСНОВНОЙ ПРОГРАММЫ ===" -ForegroundColor $Yellow
$mainSources = @(
    "$srcDir\main.c",
    "$srcDir\ngram_model.c",
    "$srcDir\tokenizer.c",
    "$srcDir\c_parser.c",
    "$srcDir\md_parser.c",
    "$srcDir\markov.c",
    "$srcDir\interactive.c",
    "$srcDir\utils.c"
)
$needRebuildMain = $Rebuild
if (-not $needRebuildMain) {
    $srcTime = (Get-ChildItem $mainSources -ErrorAction SilentlyContinue | Measure-Object -Maximum LastWriteTime).Maximum
    if ($srcTime -and (Test-Path $executable)) {
        $exeTime = (Get-Item $executable).LastWriteTime
        if ($srcTime -gt $exeTime) { $needRebuildMain = $true }
    } elseif (-not (Test-Path $executable)) {
        $needRebuildMain = $true
    }
}
if ($needRebuildMain) {
    Build-Main
} else {
    Write-Host "Основная программа уже собрана, пропускаем сборку." -ForegroundColor $Green
}

# ---- 3. Очистка старых данных ----
Write-Host "`n=== ОЧИСТКА СТАРЫХ ВЫХОДНЫХ ДАННЫХ ===" -ForegroundColor $Yellow
if (Test-Path $outputDir) { Remove-Item -Path $outputDir -Recurse -Force }
if (Test-Path $resultDir) { Remove-Item -Path $resultDir -Recurse -Force }
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
New-Item -ItemType Directory -Path $resultDir -Force | Out-Null

# ---- 4. Запуск основной программы ----
Write-Host "`n=== ЗАПУСК ОСНОВНОЙ ПРОГРАММЫ ===" -ForegroundColor $Yellow
$runArgs = @()
if ($Arguments.Count -eq 0) {
    $runArgs = @("--interactive", "--order", "1", "--temp", "0.8")
    Write-Host "Аргументы не указаны. Запуск в интерактивном режиме." -ForegroundColor $Yellow
} else {
    $runArgs = $Arguments
}
Write-Host "Запуск: $executable $($runArgs -join ' ')" -ForegroundColor $Cyan
& $executable $runArgs
if ($LASTEXITCODE -eq 0) {
    Write-Host "Программа завершена успешно." -ForegroundColor $Green
} else {
    Write-Host "Программа завершилась с кодом ошибки: $LASTEXITCODE" -ForegroundColor $Red
}