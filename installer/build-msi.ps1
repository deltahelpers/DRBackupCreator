param(
    [switch]$NoBuild,
    [string]$Version,
    [string]$WixDll = (Join-Path $env:LOCALAPPDATA "drbc-tools\wix6\pkg\tools\net6.0\any\wix.dll")
)

# Builds the per-user MSI (WiX 6 CLI, no admin required).
#   powershell -File installer\build-msi.ps1 [-Version 1.2.3]
#   -> dist\drbcWindowsInstaller.msi

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $root

if (-not $NoBuild) {
    & (Join-Path $root "build.ps1")
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit code $LASTEXITCODE)" }
}

# Default to the version declared in the .NET project.
if (-not $Version) {
    $project = Get-Content (Join-Path $root "dotnet\DRBackupCreator.csproj") -Raw
    $m = [regex]::Match($project, '<Version>([^<]+)</Version>')
    if (-not $m.Success) { throw "Could not read the project version from the .NET project; pass -Version." }
    $Version = $m.Groups[1].Value
}

# WiX 6 ships as a NuGet package; fetch it once into %LOCALAPPDATA%.
if (-not (Test-Path $WixDll)) {
    Write-Host "WiX 6 not found, downloading it..."
    # <pkg>\tools\net6.0\any\wix.dll -> <pkg>
    $pkgDir = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $WixDll)))
    New-Item -ItemType Directory -Force $pkgDir | Out-Null
    $zip = Join-Path $env:TEMP "wix.6.0.2.nupkg.zip"
    curl.exe -sSL -o $zip "https://api.nuget.org/v3-flatcontainer/wix/6.0.2/wix.6.0.2.nupkg"
    if ($LASTEXITCODE -ne 0) { throw "Could not download WiX 6 from nuget.org" }
    Expand-Archive -Path $zip -DestinationPath $pkgDir -Force
    Remove-Item $zip -Force
}

if (-not (Test-Path $WixDll)) { throw "wix.dll not found at $WixDll" }

$exe = Join-Path $root "build\bin\drbc.exe"
if (-not (Test-Path $exe)) { throw "Missing $exe - run build.ps1 first." }

New-Item -ItemType Directory -Force (Join-Path $root "dist") | Out-Null

# WiX 6 targets .NET 6; allow it to run on the installed (newer) runtime.
$env:DOTNET_ROLL_FORWARD = "LatestMajor"

$msi = Join-Path $root "dist\drbcWindowsInstaller.msi"
dotnet $WixDll build -arch x64 -d Root="$root" -d Version="$Version" -o $msi (Join-Path $root "installer\wix\Product.wxs")
if ($LASTEXITCODE -ne 0) { throw "WiX build failed (exit code $LASTEXITCODE)" }

if (Test-Path $msi) {
    Write-Host ""
    Write-Host ("Listo: " + $msi + " (" + [math]::Round((Get-Item $msi).Length / 1MB, 2) + " MB, v" + $Version + ")")
}
