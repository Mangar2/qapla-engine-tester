$tests = @(
    @{ args = "--help"; expectedCode = 0 }
    ,@{ args = "--invalid"; expectedCode = 2 }
    ,@{ args = "--logpath=log"; expectedCode = 0 }
    ,@{ args = "--logpath=log --enginelog --concurrency=20 --tc=10+0.02 --pgnoutput file=log/test.pgn append=false pv --sprt elolower=0 eloupper=40 alpha=0.05 beta=0.05 maxgames=3000 --engine conf=""Qapla 0.3.2"" --engine conf=""Spike 1.4""  --openings file=""book8ply.raw"" order=random format=raw --enginesfile=""C:\Development\qapla-engine-tester\test\engines.ini"""; expectedCode = 10 }
    #,@{ args = "--logpath=log --enginelog --concurrency=20 --tc=10+0.02 --sprt elolower=10 eloupper=30 alpha=0.05 beta=0.02 maxgames=3000 --engine conf=""Spike 1.4"" --engine conf=""Qapla 0.3.2"" --openings file=""book8ply.raw"" format=raw --enginesfile=""C:\Development\qapla-engine-tester\test\engines.ini"""; expectedCode = 10 }
    #,@{ args = "--logpath=log --enginelog --concurrency=20 --epd file=wmtest.epd maxtime=20 mintime=1 seenplies=3 minsuccess=80 --engine conf=""Qapla 0.3.2"" --enginesfile=""C:\Development\qapla-engine-tester\test\engines.ini"""; expectedCode = 10 }
    #,@{ args = "--logpath=log --enginelog --test numgames=0 nostop nooption --engine conf=""Qapla 0.3.1"" --enginesfile=""C:\Development\qapla-engine-tester\test\engines.ini"""; expectedCode = 10 }
    #,@{ args = "--logpath=log --enginelog --test numgames=0 nostop --engine conf=""Qapla 0.3.1"" --enginesfile=""C:\Development\qapla-engine-tester\test\engines.ini"""; expectedCode = 10 }
    #,@{ args = "--logpath=log --enginelog --test numgames=0 nostop --engine conf=""Qapla 0.3.2"" --enginesfile=""C:\Development\qapla-engine-tester\test\engines.ini"""; expectedCode = 0 }
    #,@{ args = "--logpath=log --test numgames=0 --engine name=Qapla0.3.2 cmd=C:\Chess\delivery\Qapla0.3.2\Qapla0.3.2-win-x86.exe"; expectedCode = 10 }
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
