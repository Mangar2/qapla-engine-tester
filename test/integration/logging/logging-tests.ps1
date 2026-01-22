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
    },
    @{
        Name = "logging-disabled-no-files"
        Description = "SPRT mit deaktiviertem Engine-Logging - keine Engine-Logdateien"
        Args = "--settingsfile=test/integration/logging/test-logging-disabled.ini"
        LogPath = "test/integration/log/logging-disabled"
        Validators = @(
            @{ Type = "exitCode"; Expected = 16 }
            @{ Type = "logFiles"; Path = ""; Pattern = "engine-*.log"; Count = 0 }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/logging-disabled' -Recurse -Force -ErrorAction SilentlyContinue"
    },
    @{
        Name = "logging-engine-trace-none"
        Description = "SPRT mit engine=true aber per-Engine trace=none - keine Engine-Logdateien"
        Args = "--settingsfile=test/integration/logging/test-logging-engine-trace-none.ini"
        LogPath = "test/integration/log/logging-engine-trace-none"
        Validators = @(
            @{ Type = "exitCode"; Expected = 16 }
            @{ Type = "logFiles"; Path = ""; Pattern = "engine-*.log"; Count = 0 }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/logging-engine-trace-none' -Recurse -Force -ErrorAction SilentlyContinue"
    },
    @{
        Name = "logging-each-trace-none"
        Description = "SPRT mit engine=true aber [each] trace=none - keine Engine-Logdateien"
        Args = "--settingsfile=test/integration/logging/test-logging-each-trace-none.ini"
        LogPath = "test/integration/log/logging-each-trace-none"
        Validators = @(
            @{ Type = "exitCode"; Expected = 16 }
            @{ Type = "logFiles"; Path = ""; Pattern = "engine-*.log"; Count = 0 }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/logging-each-trace-none' -Recurse -Force -ErrorAction SilentlyContinue"
    }
)
