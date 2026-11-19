# Copilot instructions

## Build and tests

The app targets .NET Framework 4.8 and is built with the .NET 8 SDK. From PowerShell:

```powershell
.\build.ps1
.\build.ps1 -Clean
```

`build.ps1` builds the WinForms app, compiles the standalone backup-core tests, and runs them. To run that test executable alone:

```powershell
.\build\tests\DRBackupCreator.Tests.exe
```

The app is emitted to `build\bin\drbc.exe`. There is no separate linter or formatter configured. Installer scripts are `.\installer\build-installer.ps1` (Inno Setup) and `.\installer\build-msi.ps1` (WiX 6); both build the app unless `-NoBuild` is passed.

## Architecture

- `dotnet\DRBackupCreator.csproj` is the primary application project. `Program.cs` handles startup, CLI dispatch, and file association arguments; `MainForm.cs` and `Dialogs.cs` build the standard WinForms UI.
- `BackupCore.cs` owns slot parsing/grouping, backup listing/copy/restore, legacy backup discovery, and Recycle Bin deletion. Keep these operations independent of WinForms so `dotnet.tests\` can link and exercise the core directly.
- `AppSettings.cs` persists user settings in `%LOCALAPPDATA%\DRBackupCreator\settings.ini`. `AppServices.cs` integrates Task Scheduler, `.bcconfig`/legacy `.bctheme`, and GitHub release downloads. `ThemeService.cs` applies the two standard-control palettes.
- `.bcconfig` is a small YAML-like text format, documented in `docs\FORMATS.md`. Legacy `.bctheme` colors map to the nearest light/dark palette; custom drawing and font/window geometry are not part of this port.
- `build.ps1` is the local build/test entry point; `.github\workflows\build.yml` builds the portable ZIP and both installers. The installers consume `build\bin\drbc.exe` and do not package the old native core DLL.

## Repository-specific conventions

- Target .NET Framework 4.8 and C# 7.3; avoid APIs requiring newer .NET runtimes. `Microsoft.NETFramework.ReferenceAssemblies` supplies targeting references when using the .NET SDK.
- Use `System.IO.Path`/`File`/`Directory` and Windows APIs for filesystem and shell behavior. Windows paths should not be converted to UTF-8 for ordinary file operations.
- Keep user-visible text in both English and Spanish where the existing `UiText` helper is used.
- Keep the theme selection to `dark` and `light`; apply palettes to standard controls rather than adding owner-draw implementations.
- Keep the application version in `<Version>` in `dotnet\DRBackupCreator.csproj`; build and installer scripts read or override that value.
