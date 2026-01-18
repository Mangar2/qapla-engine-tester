function Remove-TestDirectory {
    param([string]$Path, [switch]$Force)
    
    if (Test-Path -Path $Path) {
        if ($Force) {
            Remove-Item -Path $Path -Recurse -Force -ErrorAction SilentlyContinue
        } else {
            Remove-Item -Path $Path -Recurse -ErrorAction SilentlyContinue
        }
    }
}

function Clear-LogDirectory {
    param([string]$Path = "log", [string]$Pattern = "*.log")
    
    if (Test-Path -Path $Path) {
        Get-ChildItem -Path $Path -Filter $Pattern -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
    }
}
