
<p align="center">
  <a href="https://www.gnu.org/licenses/gpl-3.0.html">
    <img src="https://img.shields.io/badge/license-GPL-yellow.svg?style=plastic&logo=GNU&label=License">
  </a>
  <a href="../../releases">
    <img src="https://img.shields.io/github/downloads/deltahelpers/DRBackupCreator/total.svg?style=plastic&color=red">
  </a>
  <a href="../../releases/latest">
    <img src="https://img.shields.io/github/downloads/deltahelpers/DRBackupCreator/latest/total?style=plastic">
  </a>
</p>

> [!WARNING]
> **READ BEFORE DOWNLOADING, the only official repository/source for DRBackupCreator is "<https://github.com/deltahelpers/DRBackupCreator>", another repository is not official. Also, the only official website is "<https://deltahelpers.github.io>. You are warned."**

# DRBackupCreator (drbc)
"**D**_ELTA_**R**_UNE_ **B**_ackup_ **C**_reator_"

An actually compatible, powerful, lightweight [DELTARUNE](https://deltarune.com) backup manager written on [C/C++](https://jmeubank.github.io/tdm-gcc/download/) with [Dear ImGUI](https://github.com/ocornut/imgui) as the GUI, [DRBackupCCore]("https://github.com/deltahelpers/DRBackupCCore") as an [shared library](https://www.geeksforgeeks.org/cpp/how-to-create-a-dynamic-library-in-cpp/) (.dll), that search your saves in %LOCALAPPDATA%\DELTARUNE, and backups it on %LOCALAPPDATA%\DRBackupCreator on a user-friendly GUI.

<p align="right">
  <img src="./icon.png">
</p>

## How I install it?

For installation, go to [the releases page,](https://github.com/deltahelpers/DRBackupCreator/releases/latest) download the appropiate file, and install it with our installer. It currently only support Windows, but theres an _work-in-progress_ [Linux](https://github.com/deltahelpers/DRBackupCreatorQt) version using the [Qt](https://www.qt.io) framework.

## How I compile it?

Its _actually_ better compile on Windows, because we are using %LOCALAPPDATA% and the [windows.h](https://en.wikipedia.org/wiki/Windows.h) library. Also don't forget to compile using an [MinGW compiler](https://osdn.net/projects/mingw/).

## Windows

### Prerequisites:
Install [CMake](https://cmake.org/download/#latest) and [TDM-GCC](https://jmeubank.github.io/tdm-gcc/download/) from their respective sites, i linked them so just click the names.

### Building

1. Open Powershell from the _Start Menu_ or press **Win + R** and type: `powershell`, then do a `cd` to this folder's route. 
- You can still use your favorite Terminal Emulator for doing this.
2. Once you are on **THIS** folder, enter the command:
`.\build.ps1`
3. Or all of this commands:
`Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
cmake -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release 
  -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++"
cmake --build build -j4`

> [!NOTE]
> The script may crash/trigger an BSOD on low-end PCs, if so change the `-j4` in `cmake --build build -j4` to the number of RAM/cores your PC haves.
