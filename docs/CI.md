# Continuous integration

The GitHub Actions workflow in `.github/workflows/build.yml` builds the .NET Framework 4.8 WinForms application on `windows-latest`, runs the backup-core checks, creates the portable ZIP and builds the Inno Setup EXE and WiX MSI installers.

Pushes to `master` and `dotnet`, pull requests, and manual workflow runs build artifacts. Pushing a `v*` tag also publishes a GitHub release with:

- `drbcWindowsInstaller.exe`
- `drbcWindowsInstaller.msi`
- `DRBackupCreator-<version>-win32.zip`

The app version is read from `<Version>` in `dotnet/DRBackupCreator.csproj`. CI can override it with the workflow's manual `version` input or a version tag; the value is passed to the build and both installer scripts.

## Run the build locally

```powershell
.\build.ps1
.\installer\build-installer.ps1
.\installer\build-msi.ps1
```

The build script restores the .NET Framework reference assemblies, builds `build\bin\drbc.exe`, builds the test executable, and runs its backup-engine checks. The installer scripts expect that executable and build it themselves unless passed `-NoBuild`.

The portable ZIP contains the executable, README, and license. .NET Framework 4.8 is required to run the application.
