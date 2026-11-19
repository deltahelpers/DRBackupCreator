param(
    [switch]$Clean,
    [string]$Version
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root

try {
    if ($Clean) {
        Remove-Item -Recurse -Force (Join-Path $root "build") -ErrorAction SilentlyContinue
    }

    $project = Join-Path $root "dotnet\DRBackupCreator.csproj"
    $tests = Join-Path $root "dotnet.tests\DRBackupCreator.Tests.csproj"
    if (-not $Version) {
        [xml]$xml = Get-Content $project
        $Version = $xml.Project.PropertyGroup.Version
    }

    & dotnet build $project --configuration Release "-p:Version=$Version"
    if ($LASTEXITCODE -ne 0) { throw "Application build failed (exit code $LASTEXITCODE)" }

    & dotnet build $tests --configuration Release
    if ($LASTEXITCODE -ne 0) { throw "Test build failed (exit code $LASTEXITCODE)" }

    $testExe = Join-Path $root "build\tests\DRBackupCreator.Tests.exe"
    & $testExe
    if ($LASTEXITCODE -ne 0) { throw "Backup core tests failed (exit code $LASTEXITCODE)" }

    Write-Host ""
    Write-Host ("Build and tests succeeded: " + (Join-Path $root "build\bin\drbc.exe"))
}
finally {
    Pop-Location
}
