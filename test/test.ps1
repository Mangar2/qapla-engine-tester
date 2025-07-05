$tests = @(
    #@{ args = "--help"; expectedCode = 0 }
    ,@{ args = "--invalid"; expectedCode = 2 }
    ,@{ args = "--logpath=log"; expectedCode = 0 }
    #,@{ args = "--settingsfile=test-noerror.ini" ; expectedCode = 0 }
    #,@{ args = "--settingsfile=test-error-10.ini" ; expectedCode = 10 }
    #,@{ args = "--settingsfile=test-error-10-stop.ini" ; expectedCode = 10 }
    #,@{ args = "--settingsfile=test-error-ponder.ini" ; expectedCode = 12 }
    #,@{ args = "--settingsfile=test-20-games.ini" ; expectedCode = 0 }
    #,@{ args = "--settingsfile=test-epd-13.ini" ; expectedCode = 13 }
    #,@{ args = "--settingsfile=test-gauntlet.ini" ; expectedCode = 0 }
    #,@{ args = "--settingsfile=test-tournament-file.ini" ; expectedCode = 0 }
    #,@{ args = "--settingsfile=test-tournament-file.ini" ; expectedCode = 0 }
    #,@{ args = "--settingsfile=test-sprt-maxgames.ini" ; expectedCode = 16 }
    #,@{ args = "--settingsfile=test-sprt-15.ini" ; expectedCode = 15 }
    #,@{ args = "--settingsfile=test-sprt-14.ini" ; expectedCode = 14 }
    #,@{ args = "--settingsfile=test-sprt-14-ponder.ini" ; expectedCode = 14 }
    ,@{ args = "--settingsfile=test-sprt-rapid.ini" ; expectedCode = 15 }
)

$results = @()
Remove-Item log\*.log
Remove-Item log\*.tour
Remove-Item log\*.pgn 

foreach ($test in $tests) {
    & .\..\x64\Release\qapla-engine-tester.exe $test.args.Split(" ") --enginesfile=C:\Development\qapla-engine-tester\test\engines.ini --logpath=log
    $exitCode = $LASTEXITCODE
    if ($exitCode -eq $test.expectedCode) {
        $text = "✔ Test mit '$($test.args)' erfolgreich (Code $exitCode)"
    } else {
        $text = "✘ FEHLER bei '$($test.args)' (Code $exitCode, erwartet $($test.expectedCode))"
    }
    Write-Host $text
    $results += $text
}

Write-Host "`n--- Testergebnisse ---"
$results | ForEach-Object { Write-Host $_ }
