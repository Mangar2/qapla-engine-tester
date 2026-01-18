# SPRT Tests - Basis SPRT Tests für Regression

$sprtTests = @(
    @{
        Name = "sprt-maxgames-reached"
        Description = "SPRT Test mit maxgames erreicht (exit code 16)"
        Args = "--settingsfile=test/integration/sprt/test-sprt-maxgames.ini"
        LogPath = "test/integration/log/sprt"
        Validators = @(
            @{ Type = "exitCode"; Expected = 16 }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/sprt' -Recurse -Force -ErrorAction SilentlyContinue"
    },
    @{
        Name = "sprt-basic-h0-accepted"
        Description = "SPRT Test mit H0 accepted (exit code 15)"
        Args = "--settingsfile=test/integration/sprt/test-sprt-15.ini"
        LogPath = "test/integration/log/sprt"
        Validators = @(
            @{ Type = "exitCode"; Expected = 15 }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/sprt' -Recurse -Force -ErrorAction SilentlyContinue"
    },
    @{
        Name = "sprt-basic-h1-accepted"
        Description = "Basis SPRT Test mit H1 accepted (exit code 14)"
        Args = "--settingsfile=test/integration/sprt/test-sprt-14.ini"
        LogPath = "test/integration/log/sprt"
        Validators = @(
            @{ Type = "exitCode"; Expected = 14 }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/sprt' -Recurse -Force -ErrorAction SilentlyContinue"
    },
    @{
        Name = "sprt-with-ponder"
        Description = "SPRT mit Ponder-Option"
        Args = "--settingsfile=test/integration/sprt/test-sprt-14-ponder.ini"
        LogPath = "test/integration/log/sprt"
        Validators = @(
            @{ Type = "exitCode"; Expected = 14 }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/sprt' -Recurse -Force -ErrorAction SilentlyContinue"
    },
    @{
        Name = "sprt-rapid-mode"
        Description = "SPRT im Rapid-Modus"
        Args = "--settingsfile=test/integration/sprt/test-sprt-rapid.ini"
        LogPath = "test/integration/log/sprt"
        Validators = @(
            @{ Type = "exitCode"; Expected = 15 }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/sprt' -Recurse -Force -ErrorAction SilentlyContinue"
    }
)
