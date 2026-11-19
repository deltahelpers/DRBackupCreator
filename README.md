# DRBackupCreator

DRBackupCreator is a Windows DELTARUNE save backup manager with a native WinForms interface targeting **.NET Framework 4.8**. It discovers saves under `%LOCALAPPDATA%\DELTARUNE` and stores timestamped backups under `%LOCALAPPDATA%\DRBackupCreator` by default.

The interface uses standard WinForms controls with selectable dark and light modes. It supports save-slot listing, backup and restore, safe deletion to the Recycle Bin, a scriptable command line, daily scheduled backups, `.bctheme`/`.bcconfig` imports, and release update checks.

## Build

Requirements: Windows, the .NET 8 SDK, and an internet connection for the .NET Framework targeting reference package on first build. The application targets .NET Framework 4.8, available on supported Windows 10/11 versions.

From PowerShell at the repository root:

```powershell
.\build.ps1
.\build.ps1 -Clean
```

The script builds the app and runs its backup-core test executable. The app is emitted to `build\bin\drbc.exe`; tests are in `build\tests\DRBackupCreator.Tests.exe`.

To build only the app:

```powershell
dotnet build .\dotnet\DRBackupCreator.csproj --configuration Release
```

## Usage

```text
drbc.exe                          Opens the GUI
drbc.exe --cli list               Lists detected save slots
drbc.exe --cli backup             Creates a backup
drbc.exe --cli restore <index>    Restores a backup
drbc.exe --schedule 21:30         Schedules a daily backup
drbc.exe --schedule off           Disables the daily backup
drbc.exe --auto-backup            Runs a scheduled backup without a window
drbc.exe --config <file>          Imports configuration
drbc.exe --export-config <file>   Exports configuration
drbc.exe --theme <file>           Selects light/dark mode from a theme
drbc.exe --check-updates          Checks GitHub for a newer release
drbc.exe --download-update <dir>  Downloads and verifies the installer
drbc.exe --help                   Prints help
drbc.exe --version                Prints the version
```

The GUI's **Settings** dialog controls appearance, language, backup folder, daily scheduling, and automatic update checks. `F5`, `Ctrl+B`, `Ctrl+L`, and `Ctrl+D` refresh, create a backup, open backups, and open the save folder.

## Data and formats

The app's `settings.ini` and scheduled log stay under `%LOCALAPPDATA%\DRBackupCreator`; the backup directory can be changed in Settings. Existing backups in the legacy `%LOCALAPPDATA%\backup_*` location remain visible.

`.bcconfig` files set language, light/dark mode, backup directory, and schedule. Relative paths resolve from the config file. Legacy `.bctheme` files remain openable; their background color selects the nearest light or dark mode. See [docs/FORMATS.md](docs/FORMATS.md).

## Build installers

Build the app first, then use the existing installer scripts:

```powershell
.\installer\build-installer.ps1
.\installer\build-msi.ps1
```

They produce `dist\drbcWindowsInstaller.exe` (Inno Setup) and `dist\drbcWindowsInstaller.msi` (WiX). Both remain per-user installers, register `.bcconfig` and `.bctheme`, and leave user data untouched on uninstall. The CI workflow also creates a portable ZIP and publishes release assets for `v*` tags.

## Layout

- `dotnet\` — .NET Framework WinForms app, settings, update/scheduler services, and backup engine.
- `dotnet.tests\` — focused backup engine tests.
- `installer\` — Inno Setup and WiX installer definitions and build scripts.
- `docs\FORMATS.md` — configuration and theme file reference.

## License

GPL-3.0; see [LICENSE](LICENSE).
