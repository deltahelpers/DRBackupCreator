<p align="center">
  <a href="https://www.gnu.org/licenses/gpl-3.0.html">
    <img src="https://img.shields.io/badge/license-GPL--3.0-yellow.svg?style=plastic&logo=GNU&label=License" alt="License: GPL-3.0">
  </a>
  <a href="https://github.com/deltahelpers/DRBackupCreator/releases">
    <img src="https://img.shields.io/github/downloads/deltahelpers/DRBackupCreator/total.svg?style=plastic&color=red" alt="Total downloads">
  </a>
  <a href="https://github.com/deltahelpers/DRBackupCreator/releases/latest">
    <img src="https://img.shields.io/github/downloads/deltahelpers/DRBackupCreator/latest/total?style=plastic" alt="Latest release downloads">
  </a>
  <a href="https://github.com/deltahelpers/DRBackupCreator">
    <img src="https://img.shields.io/badge/platform-Windows-0078D4.svg?style=plastic&logo=windows11" alt="Platform: Windows">
  </a>
  <a href="https://jmeubank.github.io/tdm-gcc/">
    <img src="https://img.shields.io/badge/language-C%2B%2B20-00599C.svg?style=plastic&logo=cplusplus" alt="Language: C++20">
  </a>
</p>

> [!WARNING]
> **READ BEFORE DOWNLOADING. The only official repository/source for DRBackupCreator is <https://github.com/deltahelpers/DRBackupCreator> — any other repository is not official. The only official website is <https://deltahelpers.github.io>. You are warned.**

# DRBackupCreator (drbc)
"**D**_ELTA_**R**_UNE_ **B**_ackup_ **C**_reator_"

An actually compatible, powerful, lightweight [DELTARUNE](https://deltarune.com) backup manager written in [C/C++](https://jmeubank.github.io/tdm-gcc/download/) with a native **Win32** GUI, backed by [DRBackupCCore](https://github.com/deltahelpers/DRBackupCCore) as a [shared library](https://en.wikipedia.org/wiki/Dynamic-link_library) (`.dll`). It finds your saves in `%LOCALAPPDATA%\DELTARUNE` and stores timestamped backups in `%LOCALAPPDATA%\DRBackupCreator`, all from a friendly, dark-themed window.

<p align="right">
  <img src="./assets/icon.png" width="96" alt="DRBackupCreator icon">
</p>

> [!NOTE]
> This branch replaces the original Dear ImGui interface with a native Win32 GUI (no UI dependencies), keeping the same core engine and public API. The original functionality is preserved.

## ✨ What's inside?

- 🗂️ **Automatic slot detection** — scans `filech<chapter>_<slot>.dat` and groups them by save.
- 💾 **One-click backups** — copies every detected slot into a timestamped folder.
- 📜 **Backup browser** — restore or delete any backup from a ListView dialog.
- 🗑️ **Safe deletes** — removed backups go to the **Recycle Bin**, not the void.
- 🌍 **Bilingual UI (ES / EN)** — language picker on first launch, remembered afterwards.
- 🖥️ **Dark native UI** — owner-drawn widgets, dark title bar, DPI-aware (PerMonitorV2), flicker-free.
- ⚠️ **Game-running guard** — warns and blocks backups while `DELTARUNE.exe` is active.
- ⌨️ **Keyboard shortcuts** — full keyboard navigation for the GUI.
- 🧰 **Scriptable CLI** — `--cli` subcommands for automation.

## 📦 How do I install it?

Grab the appropriate file from [the releases page](https://github.com/deltahelpers/DRBackupCreator/releases/latest) and run the installer. Windows only for now (a _work-in-progress_ [Linux](https://github.com/deltahelpers/DRBackupCreatorQt) version exists, built with [Qt](https://www.qt.io)).

Or build it yourself — see below.

## 🔨 How do I compile it?

It's _actually_ better to compile on Windows, because the project relies on `%LOCALAPPDATA%` and the [windows.h](https://en.wikipedia.org/wiki/Windows.h) API. Use an [MinGW](https://jmeubank.github.io/tdm-gcc/download/) toolchain (tested with **TDM-GCC 10.3**).

### Prerequisites

Install [CMake](https://cmake.org/download/#latest) (≥ 3.20) and [TDM-GCC](https://jmeubank.github.io/tdm-gcc/download/) from their respective sites — the names link straight to the downloads.

### Building

1. Open **PowerShell** from the _Start Menu_ (or press **Win + R**, type `powershell`) and `cd` into this folder. Any terminal emulator works too.
2. From the project root, run:

   ```powershell
   .\build.ps1
   ```

   Clean build:

   ```powershell
   .\build.ps1 -Clean
   ```

3. Or do it by hand:

   ```powershell
   Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
   cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j 4
   ```

Output lands in `build\bin`:

| File | Description |
| --- | --- |
| `drbc.exe` | GUI + CLI |
| `DRBackupCCore.dll` | Backup engine (public API in `include/drbackup.h`) |

> [!NOTE]
> With TDM-GCC the C++ runtime is linked statically, so no extra DLLs are needed next to the executable. The script may feel heavy on low-end PCs — lower the `-j` value to match your core count.

## 🚀 Usage

```text
drbc.exe                          Opens the GUI
drbc.exe --cli                    Interactive CLI
drbc.exe --cli list               Lists detected slots
drbc.exe --cli backup             Creates a backup
drbc.exe --cli restore <index>    Restores a backup
drbc.exe --help                   Help
drbc.exe --version                Version
```

### ⌨️ GUI shortcuts

| Shortcut | Action |
| --- | --- |
| `F5` | Refresh slot list |
| `Ctrl+B` | Create backup |
| `Ctrl+L` | Open backups dialog |
| `Ctrl+D` | Open saves folder |
| `Ctrl+Shift+D` | Open backups folder |
| `Enter` | Restore selected backup |
| `Del` | Delete selected backup |
| `Esc` | Close dialog |

## 🗃️ Where are backups stored?

```text
%LOCALAPPDATA%\DRBackupCreator\backup_YYYY-MM-DD_HH-MM-SS\
    filech1_0.dat
    filech1_5.dat
    ...
```

`listAllBackups()` also detects legacy backups created directly in `%LOCALAPPDATA%\backup_*` by the original CLI.

## 🧱 Project structure

```text
include/
  drbackup.h          public core API (structs and functions)
  version.h
src/
  core.cpp            backups, listing, restore
  paths.cpp           %LOCALAPPDATA%, DELTARUNE and backup paths
  slots.cpp           slot parsing and grouping
  process.cpp         DELTARUNE.exe detection
  main.cpp            entry point (GUI / CLI / help)
  cli.cpp             interactive and subcommand CLI
  ui/
    gui.cpp           main window, layout, shortcuts
    dialogs.cpp       backups, confirm, language and about dialogs
    theme.cpp/.h      dark painting, owner-draw buttons, DPI
    i18n.cpp/.h       ES/EN strings and language persistence
    app.rc            icon and version resource
    app.manifest      Common Controls v6, DPI, UTF-8
tests/
  core_tests.cpp      core test suite (no GUI)
assets/
  app.ico, icon.png   multi-resolution icon
```

## 🔌 Core API

`include/drbackup.h` exposes:

```cpp
std::optional<fs::path> getLocalAppData();
std::optional<fs::path> getDeltaruneDir(bool createIfMissing = false);
std::optional<fs::path> getBackupDir(bool createIfMissing = false);

std::optional<SlotFile> parseSlotFile(const fs::path& p);
std::vector<SlotFile>   scanSlots(const fs::path& dir);
void                    groupBySave(std::vector<SlotFile>& slots);

bool isGameRunning();

std::string                     createBackup(const fs::path& backupDir,
                                             const std::vector<SlotFile>& slots);
std::vector<BackupInfo>         listBackups(const fs::path& parentDir);
std::vector<BackupInfo>         listAllBackups();
bool                            restoreBackup(const fs::path& backupPath,
                                              const fs::path& targetDir,
                                              int& restoredCount);

std::string formatFileSize(uintmax_t bytes);
std::string getCurrentTimestamp();
```

## 🧪 Tests

```powershell
ctest --test-dir build --output-on-failure
```

Covers slot parsing, grouping order, backup creation/listing/restore and helpers, using temporary directories.

## 📦 Packaging

Install rules, CPack and an [Inno Setup](https://jrsoftware.org/isinfo.php) script are included:

```powershell
cmake --install build --prefix dist        # raw files
cpack --config build/CPackConfig.cmake     # portable ZIP

.\installer\build-installer.ps1            # installer -> dist\DRBackupCreator-<version>-setup.exe
```

The installer is per-user by default (installs to `%LOCALAPPDATA%\Programs\DRBackupCreator`),
ships English and Spanish wizard languages, creates Start Menu/optional desktop icons,
and leaves `%LOCALAPPDATA%\DRBackupCreator` (backups and settings) untouched on uninstall.

## 🩹 Notes about the original

This port fixes two bugs from the original project:

1. The backup filter used `name.compare(0, 8, "backup_")`, which never matched (the prefix is 7 characters), so no backup was ever listed. It now uses `rfind("backup_", 0)`.
2. On some MinGW toolchains `fs::copy_file(..., overwrite_existing)` fails with "File exists"; it now uses `CopyFileW(..., FALSE)`.

It also unifies the backup folder between GUI and CLI (`%LOCALAPPDATA%\DRBackupCreator`) while keeping compatibility with the legacy path.

## 📄 License

GPL-3.0 — see [LICENSE](LICENSE). Originally made by DeltaHelpers.

---

<p align="center">
  <sub>Made with 🧡 for DELTARUNE players who hate losing their saves.</sub>
</p>
