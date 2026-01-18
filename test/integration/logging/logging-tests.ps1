# Logging Tests - Validiert globales vs. per-Engine Logging

$loggingTests = @(
    @{
        Name = "logging-global-single-file"
        Description = "SPRT mit globalem Logging - genau eine Log-Datei für alle Engines"
        Args = "--settingsfile=test/integration/logging/test-logging-global.ini"
        LogPath = "test/integration/log/logging-global"
        Validators = @(
            @{ Type = "exitCode"; Expected = 16 }
            @{ Type = "logFiles"; Path = ""; Pattern = "engine-*.log"; Count = 1; Content = "(bestmove|info depth)" }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/logging-global' -Recurse -Force -ErrorAction SilentlyContinue"
    },
    @{
        Name = "logging-per-engine-multiple-files"
        Description = "SPRT mit per-Engine Logging - separate Log-Datei pro Engine"
        Args = "--settingsfile=test/integration/logging/test-logging-per-engine.ini"
        LogPath = "test/integration/log/logging-per-engine"
        Validators = @(
            @{ Type = "exitCode"; Expected = 16 }
            @{ Type = "logFiles"; Path = ""; Pattern = "engine-*.log"; Count = 2; Content = "(bestmove|info depth)" }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/logging-per-engine' -Recurse -Force -ErrorAction SilentlyContinue"
    }
)
