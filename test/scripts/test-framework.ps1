function Validate-ExitCode {
    param([int]$ActualCode, [int]$ExpectedCode, [string]$TestName)
    
    if ($ActualCode -eq $ExpectedCode) {
        Write-Host "  [OK] Exit Code: $ActualCode" -ForegroundColor Green
        return $true
    } else {
        Write-Host "  [FAIL] Exit Code: $ActualCode (expected: $ExpectedCode)" -ForegroundColor Red
        return $false
    }
}

function Validate-LogFiles {
    param([string]$Path, [string]$Pattern, [int]$ExpectedCount, [string]$ContentPattern, [string]$TestName)
    
    if (-not (Test-Path $Path)) {
        Write-Host "  [FAIL] Log directory not found: $Path" -ForegroundColor Red
        return $false
    }
    
    $logFiles = @(Get-ChildItem -Path $Path -Filter $Pattern -File -ErrorAction SilentlyContinue)
    $actualCount = $logFiles.Count
    
    if ($actualCount -ne $ExpectedCount) {
        Write-Host "  [FAIL] Log file count: $actualCount (expected: $ExpectedCount)" -ForegroundColor Red
        return $false
    }
    
    Write-Host "  [OK] Log files: $actualCount found (expected: $ExpectedCount)" -ForegroundColor Green
    
    if ($ContentPattern) {
        $allHaveContent = $true
        foreach ($file in $logFiles) {
            $content = Get-Content -Path $file.FullName -Raw -ErrorAction SilentlyContinue
            if ($content -notmatch $ContentPattern) {
                Write-Host "  [FAIL] Log file '$($file.Name)' missing content: '$ContentPattern'" -ForegroundColor Red
                $allHaveContent = $false
            } else {
                Write-Host "  [OK] Log file '$($file.Name)' has expected content" -ForegroundColor Green
            }
        }
        return $allHaveContent
    }
    
    return $true
}

function Validate-FileExists {
    param([string]$Path, [string]$TestName)
    
    if (Test-Path -Path $Path) {
        Write-Host "  [OK] File exists: $Path" -ForegroundColor Green
        return $true
    } else {
        Write-Host "  [FAIL] File not found: $Path" -ForegroundColor Red
        return $false
    }
}

function Invoke-Test {
    param([hashtable]$Test)
    
    Write-Host ""
    Write-Host "  Test: $($Test.Name)" -ForegroundColor Cyan
    
    if ($Test.Cleanup) {
        Invoke-Expression $Test.Cleanup
    }
    
    $logPath = if ($Test.LogPath) { $Test.LogPath } else { "log" }
    if (-not (Test-Path $logPath)) {
        New-Item -ItemType Directory -Path $logPath -Force | Out-Null
    }
    
    $cliArgs = @($Test.Args.Split(" "))
    
    Write-Host "  Running: build\default\qapla-engine-tester.exe $($cliArgs -join ' ')" -ForegroundColor Gray
    Write-Host ""
    
    & "build\default\qapla-engine-tester.exe" $cliArgs 2>&1 | ForEach-Object { Write-Host "    $_" }
    $exitCode = $LASTEXITCODE
    
    Write-Host ""
    
    if ($Test.Validators) {
        foreach ($validator in $Test.Validators) {
            $result = $false
            
            switch ($validator.Type) {
                "exitCode" {
                    $result = Validate-ExitCode -ActualCode $exitCode -ExpectedCode $validator.Expected -TestName $Test.Name
                }
                "logFiles" {
                    $result = Validate-LogFiles -Path "$logPath/$($validator.Path)" -Pattern $validator.Pattern -ExpectedCount $validator.Count -ContentPattern $validator.Content -TestName $Test.Name
                }
                "fileExists" {
                    $result = Validate-FileExists -Path $validator.Path -TestName $Test.Name
                }
                default {
                    Write-Host "  [SKIP] Unknown validator: $($validator.Type)" -ForegroundColor Yellow
                }
            }
            
            if (-not $result) {
                $allPassed = $false
            }
        }
    }
    
    Write-Host ""
    if ($allPassed) {
        Write-Host "  [PASS] $($Test.Name)" -ForegroundColor Green
        return $true
    } else {
        Write-Host "  [FAIL] $($Test.Name)" -ForegroundColor Red
        return $false
    }
}
