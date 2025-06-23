$tests = @(
    @{ args = "--help"; expectedCode = 0 }
    #,@{ args = "--invalid"; expectedCode = 2 }
    #,@{ args = "--logpath=log"; expectedCode = 0 }
    #,@{ args = "--test numgames=0 nostop nooption --engine conf=""Qapla 0.3.1"" --enginesfile=C:\Development\qapla-engine-tester\test\engines.ini --logpath=log --enginelog" ; expectedCode = 0 }
    #,@{ args = "--test numgames=0 nostop --engine conf=""Qapla 0.3.1"" --enginesfile=""C:\Development\qapla-engine-tester\test\engines.ini"" --logpath=log --enginelog"; expectedCode = 10 }
    #,@{ args = "--test numgames=20 nostop --engine conf=""Qapla 0.3.2"" --enginesfile=""C:\Development\qapla-engine-tester\test\engines.ini"" --logpath=log --enginelog "; expectedCode = 11 }
    #,@{ args = "--test numgames=20 --engine name=Qapla0.3.2 cmd=C:\Chess\delivery\Qapla0.3.2\Qapla0.3.2-win-x86.exe --logpath=log "; expectedCode = 10 }
    #,@{ args = "--tournament type=gauntlet resultfile=log/tournamet.tour games=8 repeat=2 --engine conf=""Qapla 0.3.2"" gauntlet --engine conf=""Qapla 0.3.2"" trace=all --engine conf=""Qapla 0.3.1"" --enginesfile=C:\Development\qapla-engine-tester\test\engines.ini --logpath=log --enginelog --pgnoutput file=log/test.pgn --each tc=10+0.02 --openings order=random file=book8ply.raw format=raw"; expectedCode = 0 }
    #--tournament type=gauntlet resultfile=log/tournamet.txt games=12 repeat=2 rounds=10 --engine conf="Qapla 0.3.2" gauntlet --engine conf="Qapla 0.3.2" trace=all --engine conf="Qapla 0.3.1" --enginesfile=C:\Development\qapla-engine-tester\test\engines.ini --logpath=log --enginelog --pgnoutput file=log/test.pgn --each tc=10+0.02 --openings order=random file=test/book8ply.raw format=raw
    ,@{ args = "--logpath=log --enginelog --concurrency=20 --each tc=10+0.02 --epd file=wmtest.epd maxtime=20 mintime=1 seenplies=3 minsuccess=80 --engine conf=""Qapla 0.3.2"" --enginesfile=""C:\Development\qapla-engine-tester\test\engines.ini"""; expectedCode = 13 }
    ,@{ args = "--logpath=log --concurrency=20 --each tc=10+0.02 --sprt elolower=0 eloupper=10 alpha=0.05 beta=0.05 maxgames=300 --engine conf=""Qapla 0.3.1"" --engine conf=""Qapla 0.3.2"" --openings file=""book8ply.raw"" format=raw --enginesfile=""C:\Development\qapla-engine-tester\test\engines.ini"""; expectedCode = 16 }
    ,@{ args = "--logpath=log --concurrency=20 --each tc=10+0.02 --pgnoutput file=log/test.pgn append=false pv --sprt elolower=0 eloupper=40 alpha=0.05 beta=0.05 maxgames=3000 --engine conf=""Qapla 0.3.2"" --engine conf=""Spike 1.4""  --openings file=""book8ply.raw"" order=random format=raw --enginesfile=""C:\Development\qapla-engine-tester\test\engines.ini"""; expectedCode = 15 }
    ,@{ args = "--logpath=log --concurrency=20 --each tc=10+0.02 --pgnoutput file=log/test.pgn append pv --sprt elolower=0 eloupper=30 alpha=0.05 beta=0.02 maxgames=3000 --engine conf=""Spike 1.4"" --engine conf=""Qapla 0.3.2"" --openings file=""book8ply.raw"" format=raw --enginesfile=""C:\Development\qapla-engine-tester\test\engines.ini"""; expectedCode = 14 },
    ,@{ args = "--logpath=log --concurrency=10 --each tc=10+0.02 --pgnoutput file=log/debug.pgn append=false --enginelog --sprt elolower=50 eloupper=60 alpha=0.05 beta=0.05 maxgames=3000 --engine conf=""Qapla 0.3.2"" ponder --engine conf=""Qapla 0.3.2"" --openings file=book8ply.raw format=raw --enginesfile=engines.ini"; expectedCode = 11 }
    #--logpath=log --concurrency=10 --each tc=10+0.02 --pgnoutput file=log/debug.pgn append=false --enginelog --sprt elolower=50 eloupper=60 alpha=0.05 beta=0.05 maxgames=3000 --engine conf="Qapla 0.3.2" ponder --engine conf="Qapla 0.3.2" --openings file=test/book8ply.raw format=raw --enginesfile=test/engines.ini

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
