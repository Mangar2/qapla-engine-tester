# UTF-8 encoding for special characters
$tournamentsDir = "c:\development\qapla-engine-tester\test\tournaments"
$exePath = "c:\development\qapla-engine-tester\build\release\qapla-engine-tester.exe"

# Alle Dateien im tournaments-Verzeichnis durchlaufen
Get-ChildItem -Path $tournamentsDir -File | ForEach-Object {
    $fileName = $_.Name
    $filePath = $_.FullName
    
    Write-Host "Starte Tournament: $fileName" -ForegroundColor Green
    
    # Befehl ausführen
    & $exePath --sprt file=$filePath --concurrency=15
    
    # Return Codes interpretieren
    switch ($LASTEXITCODE) {
        0  { Write-Host "[OK] Erfolgreich abgeschlossen: $fileName (NoError)" -ForegroundColor Green }
        14 { Write-Host "[OK] H1 akzeptiert (staerkere Engine): $fileName" -ForegroundColor Cyan }
        15 { Write-Host "[OK] H0 akzeptiert (kein signifikanter Unterschied): $fileName" -ForegroundColor Cyan }
        16 { Write-Host "[UNDEF] Unentschieden (Result konnte nicht entschieden werden): $fileName" -ForegroundColor Yellow }
        1  { Write-Host "[ERROR] Allgemeiner Fehler bei: $fileName" -ForegroundColor Red }
        2  { Write-Host "[ERROR] Ungueltige Parameter bei: $fileName" -ForegroundColor Red }
        10 { Write-Host "[ERROR] Engine-Fehler bei: $fileName" -ForegroundColor Red }
        11 { Write-Host "[ERROR] Engine-Misbehaviour bei: $fileName" -ForegroundColor Red }
        12 { Write-Host "[WARN] Engine-Warnung bei: $fileName" -ForegroundColor Yellow }
        13 { Write-Host "[WARN] Ziel nicht erreicht bei: $fileName" -ForegroundColor Yellow }
        default { Write-Host "[ERROR] Unbekannter Fehler bei: $fileName (Exit Code: $LASTEXITCODE)" -ForegroundColor Red }
    }
    
    Write-Host ""
}

Write-Host "Alle Tournaments abgeschlossen!" -ForegroundColor Cyan
