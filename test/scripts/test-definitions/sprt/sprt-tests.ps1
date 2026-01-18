# SPRT Tests - Basis SPRT Tests für Regression

$sprtTests = @(
    @{
        Name = "sprt-basic-h1-accepted"
        Description = "Basis SPRT Test mit H1 accepted (exit code 14)"
        Args = "--settingsfile=test/scripts/test-definitions/sprt/test-sprt-14.ini"
        LogPath = "test/scripts/log/sprt"
        Validators = @(
            @{ Type = "exitCode"; Expected = 14 }
        )
        Cleanup = "Remove-Item -Path 'test/scripts/log/sprt' -Recurse -Force -ErrorAction SilentlyContinue"
    },
    @{
        Name = "sprt-with-ponder"
        Description = "SPRT mit Ponder-Option"
        Args = "--settingsfile=test/scripts/test-definitions/sprt/test-sprt-14-ponder.ini"
        LogPath = "test/scripts/log/sprt"
        Validators = @(
            @{ Type = "exitCode"; Expected = 14 }
        )
        Cleanup = "Remove-Item -Path 'test/scripts/log/sprt' -Recurse -Force -ErrorAction SilentlyContinue"
    },
    @{
        Name = "sprt-rapid-mode"
        Description = "SPRT im Rapid-Modus"
        Args = "--settingsfile=test/scripts/test-definitions/sprt/test-sprt-rapid.ini"
        LogPath = "test/scripts/log/sprt"
        Validators = @(
            @{ Type = "exitCode"; Expected = 15 }
        )
        Cleanup = "Remove-Item -Path 'test/scripts/log/sprt' -Recurse -Force -ErrorAction SilentlyContinue"
    }
)
