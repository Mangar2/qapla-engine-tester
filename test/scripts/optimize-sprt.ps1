# SPRT Parameter Optimization Script
# Optimizes pstKingEgScale UCI parameter using binary search with SPRT tournaments

$ErrorActionPreference = "Stop"

# Configuration
$execPath = ".\build\release\qapla-engine-tester.exe"
$settingsFile = "test\tournaments\sprt.ini"
$parameterName = "pstKingEgScale"
$engineAName = "Qapla 0.4.0 A"
$engineBName = "Qapla 0.4.0 B"
$engineCmd = "C:\Development\Qapla2\x64\Release\Qapla.exe"

# Search bounds
$minValue = 300
$maxValue = 1000
$bestValue = 500

# Results storage
$testResults = @()

# Function to run SPRT test
function Test-SPRTValue {
    param(
        [int]$testValue,
        [int]$baselineValue
    )
    
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "Testing: $testValue vs $baselineValue (baseline)" -ForegroundColor Yellow
    Write-Host "========================================" -ForegroundColor Cyan
    
    $cmd = "$execPath --settingsfile=`"$settingsFile`" --concurrency=16 --engine name=`"$engineBName`" cmd=`"$engineCmd`" gauntlet=false option.$parameterName=$baselineValue --engine name=`"$engineAName`" cmd=`"$engineCmd`" gauntlet=true option.$parameterName=$testValue"
    
    Write-Host "Command: $cmd" -ForegroundColor Gray
    Write-Host ""
    
    & $execPath --settingsfile="$settingsFile" --concurrency=16 --engine name="$engineBName" cmd="$engineCmd" gauntlet=false option.$parameterName=$baselineValue --engine name="$engineAName" cmd="$engineCmd" gauntlet=true option.$parameterName=$testValue | Out-Host
    $exitCode = $LASTEXITCODE
    
    $result = switch ($exitCode) {
        0  { "NoError" }
        14 { "H1_Accepted" }
        15 { "H0_Accepted" }
        16 { "Undecided" }
        10 { "EngineError" }
        11 { "EngineMissbehaviour" }
        default { "Unknown_$exitCode" }
    }
    
    Write-Host ""
    Write-Host "Result: $result (Exit Code: $exitCode)" -ForegroundColor $(if ($exitCode -eq 14) { "Green" } elseif ($exitCode -in @(15,16)) { "Yellow" } else { "Red" })
    
    return @{
        TestValue = $testValue
        BaselineValue = $baselineValue
        ExitCode = $exitCode
        Result = $result
        IsImprovement = ($exitCode -eq 14)
        IsError = ($exitCode -in @(10, 11))
    }
}

# Binary search function - searches in one direction until no improvement
function Search-Range {
    param(
        [int]$low,
        [int]$high,
        [string]$direction
    )
    
    while ($low -le $high) {
        $mid = [math]::Floor(($low + $high) / 2)
        
        # Avoid testing baseline value against itself
        if ($mid -eq $script:bestValue) {
            if ($direction -eq "upper") {
                $mid = $script:bestValue + 1
            } else {
                $mid = $script:bestValue - 1
            }
        }
        
        # Check if we're out of bounds or same as best
        if ($mid -lt $minValue -or $mid -gt $maxValue -or $mid -eq $script:bestValue) {
            break
        }
        
        # Additional safety: ensure low <= high after adjustment
        if (($direction -eq "upper" -and $mid -gt $high) -or ($direction -eq "lower" -and $mid -lt $low)) {
            break
        }
        
        $result = Test-SPRTValue -testValue $mid -baselineValue $script:bestValue
        $script:testResults += $result
        
        # Handle errors
        if ($result.IsError) {
            Write-Host "Engine error occurred. Aborting search in this direction." -ForegroundColor Red
            break
        }
        
        # If improvement found
        if ($result.IsImprovement) {
            Write-Host "→ Improvement found! New best value: $mid" -ForegroundColor Green
            $oldBest = $script:bestValue
            $script:bestValue = $mid
            
            # Update search range: continue in same direction from new best
            if ($direction -eq "upper") {
                $low = $mid + 1
                # Keep searching upward: [mid+1, high]
            } else {
                $high = $mid - 1
                # Keep searching downward: [low, mid-1]
            }
        } else {
            # No improvement (H0 or Undecided) - search opposite side
            Write-Host "→ No improvement detected." -ForegroundColor Yellow
            
            if ($direction -eq "upper") {
                # Value too high, search lower half: [low, mid-1]
                $high = $mid - 1
            } else {
                # Value too low, search upper half: [mid+1, high]
                $low = $mid + 1
            }
        }
    }
}

# Main optimization loop
Write-Host "=============================================" -ForegroundColor Magenta
Write-Host "SPRT Parameter Optimization: $parameterName" -ForegroundColor Magenta
Write-Host "=============================================" -ForegroundColor Magenta
Write-Host "Initial value: $bestValue" -ForegroundColor Cyan
Write-Host "Search range: $minValue - $maxValue" -ForegroundColor Cyan
Write-Host ""

$initialBest = $bestValue

# Test upper range first (500 → 1000)
Write-Host ""
Write-Host "=== Phase 1: Searching upper range ($bestValue → $maxValue) ===" -ForegroundColor Magenta
$upperLow = $bestValue + 1
$upperHigh = $maxValue
if ($upperLow -le $upperHigh) {
    Search-Range -low $upperLow -high $upperHigh -direction "upper"
} else {
    Write-Host "No upper range to search." -ForegroundColor Yellow
}

# Test lower range (300 → current best)
Write-Host ""
Write-Host "=== Phase 2: Searching lower range ($minValue → $bestValue) ===" -ForegroundColor Magenta
$lowerLow = $minValue
$lowerHigh = $bestValue - 1
if ($lowerLow -le $lowerHigh) {
    Search-Range -low $lowerLow -high $lowerHigh -direction "lower"
} else {
    Write-Host "No lower range to search." -ForegroundColor Yellow
}

# Final summary
Write-Host ""
Write-Host ""
Write-Host "=============================================" -ForegroundColor Magenta
Write-Host "          OPTIMIZATION COMPLETE" -ForegroundColor Magenta
Write-Host "=============================================" -ForegroundColor Magenta
Write-Host ""
Write-Host "Initial value: $initialBest" -ForegroundColor Cyan
Write-Host "Optimal value: $bestValue" -ForegroundColor Green
Write-Host "Improvement:   $(if ($bestValue -ne $initialBest) { "$($bestValue - $initialBest) ($(if ($bestValue -gt $initialBest) { '+' })$([math]::Round((($bestValue - $initialBest) / $initialBest) * 100, 1))%)" } else { "None" })" -ForegroundColor $(if ($bestValue -ne $initialBest) { "Green" } else { "Yellow" })
Write-Host ""
Write-Host "Total tests run: $($testResults.Count)" -ForegroundColor Cyan
Write-Host ""
Write-Host "=============================================" -ForegroundColor Magenta
Write-Host "          DETAILED TEST RESULTS" -ForegroundColor Magenta
Write-Host "=============================================" -ForegroundColor Magenta
Write-Host ""

# Display all test results in table format
Write-Host "Test#  | Test Value | Baseline   | Result        | Improvement" -ForegroundColor White
Write-Host "-------|------------|------------|---------------|------------" -ForegroundColor White

for ($i = 0; $i -lt $testResults.Count; $i++) {
    $r = $testResults[$i]
    $improvement = if ($r.IsImprovement) { "YES" } else { "NO" }
    $color = if ($r.IsImprovement) { "Green" } elseif ($r.IsError) { "Red" } else { "Yellow" }
    
    Write-Host ("{0,5}  | {1,10} | {2,10} | {3,-13} | {4}" -f `
        ($i + 1), $r.TestValue, $r.BaselineValue, $r.Result, $improvement) -ForegroundColor $color
}

Write-Host ""
Write-Host "=============================================" -ForegroundColor Magenta
Write-Host "Final recommendation: Use option.$parameterName=$bestValue" -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Magenta
