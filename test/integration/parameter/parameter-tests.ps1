# Parameter Tests - Validates parameter handling

return ,@(
    @{
        Name = "parameter-concurrency-basic"
        Description = "SPRT with concurrency=1 - single game execution"
        Args = "--settingsfile=test/integration/parameter/test-parameter-concurrency.ini"
        LogPath = "test/integration/log/parameter"
        Validators = @(
            @{ Type = "exitCode"; Expected = 10 }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/parameter' -Recurse -Force -ErrorAction SilentlyContinue"    },
    @{
        Name = "parameter-missing-mandatory"
        Description = "SPRT with missing mandatory parameter (openings file)"
        Args = "--settingsfile=test/integration/parameter/test-parameter-missing-mandatory.ini"
        LogPath = "test/integration/log/parameter"
        Validators = @(
            @{ Type = "exitCode"; Expected = 2 }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/parameter' -Recurse -Force -ErrorAction SilentlyContinue"    },
    @{
        Name = "parameter-cmdline-override"
        Description = "SPRT with command line parameter overriding missing ini parameter"
        Args = "--settingsfile=test/integration/parameter/test-parameter-missing-mandatory.ini --openings file=test/opening/book8ply.raw"
        LogPath = "test/integration/log/parameter"
        Validators = @(
            @{ Type = "exitCode"; Expected = 10 }
        )
        Cleanup = "Remove-Item -Path 'test/integration/log/parameter' -Recurse -Force -ErrorAction SilentlyContinue"    }
)
