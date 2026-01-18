param(
    [string]$Filter = "*",
    [string]$TestName,
    [switch]$ListTests
)

. "./test/scripts/test-framework.ps1"
. "./test/scripts/test-helpers.ps1"

# Lade alle Test-Gruppen aus test-definitions/*/
$testDirs = Get-ChildItem -Path "./test/scripts/test-definitions" -Directory
foreach ($dir in $testDirs) {
    $testFile = Join-Path $dir.FullName "$($dir.Name)-tests.ps1"
    if (Test-Path $testFile) {
        . $testFile
    }
}

# Sammle alle Tests aus den geladenen Variablen
$allTests = @()
if (Get-Variable -Name "loggingTests" -ErrorAction SilentlyContinue) { $allTests += $loggingTests }
if (Get-Variable -Name "sprtTests" -ErrorAction SilentlyContinue) { $allTests += $sprtTests }

$testsToRun = @()

if ($TestName) {
    $testsToRun = $allTests | Where-Object { $_.Name -eq $TestName }
    if ($testsToRun.Count -eq 0) {
        Write-Host "Test not found: $TestName" -ForegroundColor Red
        exit 1
    }
} else {
    $testsToRun = $allTests | Where-Object { $_.Name -like $Filter }
}

if ($ListTests) {
    Write-Host "Available tests:" -ForegroundColor Cyan
    Write-Host ""
    foreach ($test in $allTests) {
        Write-Host "  + $($test.Name)" -ForegroundColor Green
        Write-Host "    $($test.Description)" -ForegroundColor Gray
    }
    exit 0
}

if ($testsToRun.Count -eq 0) {
    Write-Host "No tests found for filter: $Filter" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Qapla Engine Tester - Test Runner" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Tests to run: $($testsToRun.Count)" -ForegroundColor Yellow
Write-Host ""

$results = @()
$passed = 0
$failed = 0

foreach ($test in $testsToRun) {
    $result = Invoke-Test $test
    
    if ($result) {
        $passed++
        $results += @{ Name = $test.Name; Passed = $true }
    } else {
        $failed++
        $results += @{ Name = $test.Name; Passed = $false }
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Test Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Total: $($results.Count) | Passed: $passed | Failed: $failed" -ForegroundColor Yellow

if ($failed -gt 0) {
    Write-Host ""
    Write-Host "Failed tests:" -ForegroundColor Red
    foreach ($result in $results | Where-Object { -not $_.Passed }) {
        Write-Host "  - $($result.Name)" -ForegroundColor Red
    }
}

Write-Host ""

if ($failed -eq 0) {
    Write-Host "All tests passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "$failed test(s) failed" -ForegroundColor Red
    exit 1
}
