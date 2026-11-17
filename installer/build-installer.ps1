param(
    [switch]$NoBuild,
    [string]$Iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $root

if (-not $NoBuild) {
    & (Join-Path $root "build.ps1")
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit code $LASTEXITCODE)" }
}

if (-not (Test-Path $Iscc)) {
    throw "ISCC.exe not found. Pass -Iscc <path to ISCC.exe> or install Inno Setup 6."
}

$iss = Join-Path $root "installer\DRBackupCreator.iss"
& $Iscc $iss
if ($LASTEXITCODE -ne 0) { throw "Inno Setup compile failed (exit code $LASTEXITCODE)" }

$setup = Get-ChildItem (Join-Path $root "dist") -Filter "drbcWindowsInstaller.exe" |
         Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($setup) {
    Write-Host ""
    Write-Host ("Listo: " + $setup.FullName + " (" + [math]::Round($setup.Length / 1MB, 2) + " MB)")
}
