# SPRT Tests - Beispiel-Migration von bestehenden Tests in neue Struktur

$sprtTests = @(
    @{
        Name = "sprt-basic-success"
        Description = "Basis SPRT Test mit maxgames=1"
        Args = "--settingsfile=test-sprt-14.ini"
        LogPath = "log/sprt"
        Validators = @(
            @{ Type = "exitCode"; Expected = 14 }
        )
        Cleanup = "Remove-Item -Path 'log/sprt' -Recurse -Force -ErrorAction SilentlyContinue"
    },
    @{
        Name = "sprt-with-ponder"
        Description = "SPRT mit Ponder-Option"
        Args = "--settingsfile=test-sprt-14-ponder.ini"
        LogPath = "log/sprt-ponder"
        Validators = @(
            @{ Type = "exitCode"; Expected = 14 }
        )
        Cleanup = "Remove-Item -Path 'log/sprt-ponder' -Recurse -Force -ErrorAction SilentlyContinue"
    },
    @{
        Name = "sprt-rapid-mode"
        Description = "SPRT im Rapid-Modus"
        Args = "--settingsfile=test-sprt-rapid.ini"
        LogPath = "log/sprt-rapid"
        Validators = @(
            @{ Type = "exitCode"; Expected = 15 }
        )
        Cleanup = "Remove-Item -Path 'log/sprt-rapid' -Recurse -Force -ErrorAction SilentlyContinue"
    }
)
