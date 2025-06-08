$tests = @(
    @{ args = "--help"; expectedCode = 0 },
    @{ args = "--invalid"; expectedCode = 2 },
    @{ args = "--logpath=log"; expectedCode = 0 },
    @{ args = "--logpath=log --test numgames=0 --engine name=Qapla0.3.2 cmd=C:\Chess\delivery\Qapla0.3.2\Qapla0.3.2-win-x86.exe"; expectedCode = 0 }
)

$results = @()

foreach ($test in $tests) {
    & .\..\x64\Release\qapla-engine-tester.exe $test.args.Split(" ") 
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq $test.expectedCode) {
        $results += "✔ Test mit '$($test.args)' erfolgreich (Code $exitCode)"
    } else {
        $results += "✘ FEHLER bei '$($test.args)' (Code $exitCode, erwartet $($test.expectedCode))"
    }
}

Write-Host "`n--- Testergebnisse ---"
$results | ForEach-Object { Write-Host $_ }
