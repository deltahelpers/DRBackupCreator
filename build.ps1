param(
    [int]$Jobs = 2,
    [string]$BuildDir = "build",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root

# Native tools may write warnings to stderr; PowerShell would treat those as
# terminating errors, so run them with a relaxed preference and check the
# exit code ourselves.
function Invoke-Native {
    param(
        [scriptblock]$Command,
        [string]$What
    )

    $ErrorActionPreference = "Continue"
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$What failed (exit code $LASTEXITCODE)"
    }
}

try {
    $needsConfigure = $Clean -or -not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))

    if ($Clean) {
        Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
    }

    if ($needsConfigure) {
        Invoke-Native {
            cmake -B $BuildDir -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
        } "CMake configure"
    }

    Invoke-Native { cmake --build $BuildDir -j $Jobs } "Build"

    Write-Host ""
    Write-Host "Listo: $BuildDir\bin\drbc.exe"
}
finally {
    Pop-Location
}
