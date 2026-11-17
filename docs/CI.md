# Continuous integration

Everything in this project is built by GitHub Actions: the Win32 app, the tests, the
portable ZIP and both installers. Pushing a `v*` tag also publishes a GitHub release with
the files attached, using the same asset names every time so the download links never
change.

- Workflow: [`.github/workflows/build.yml`](../.github/workflows/build.yml)
- Runner: `windows-latest`
- Toolchain: MSYS2 **UCRT64** MinGW-w64 GCC + CMake + Ninja

## What runs

```
push / PR / manual ──► windows
                        ├─ resolve version
                        ├─ cmake configure + build
                        ├─ ctest
                        ├─ cpack (portable ZIP)
                        ├─ Inno Setup   -> dist\drbcWindowsInstaller.exe
                        ├─ WiX 6        -> dist\drbcWindowsInstaller.msi
                        ├─ check imports (no MinGW runtime DLLs)
                        └─ upload artifact "drbc-windows"

tag v* ─────────────► release
                        └─ attach the three files to the GitHub release
```

### Jobs

| Job | Runs on | Purpose |
| --- | --- | --- |
| `windows` | every trigger | Build, test, package, upload artifacts |
| `release` | only `v*` tags | Attach the files to the release for that tag |

### Artifacts

Every run keeps an artifact named **`drbc-windows`** (14 days) containing:

```text
drbcWindowsInstaller.exe        # Inno Setup, per-user
drbcWindowsInstaller.msi        # WiX 6, per-user
DRBackupCreator-<version>-win32.zip   # portable build (CPack)
```

The two installer names are intentionally fixed: a release always publishes
`drbcWindowsInstaller.exe` / `.msi`, which is what makes these links stable forever:

```text
https://github.com/deltahelpers/DRBackupCreator/releases/latest/download/drbcWindowsInstaller.exe
https://github.com/deltahelpers/DRBackupCreator/releases/latest/download/drbcWindowsInstaller.msi
```

## Versioning

The version is resolved in this order:

1. **Tag** — pushing `v2.0.0` builds `2.0.0` (the leading `v` is stripped).
2. **Manual run** — the `version` input of *Run workflow*.
3. **`CMakeLists.txt`** — `project(DRBackupCreator VERSION x.y.z)`.

CI passes the value to CMake as `-DDRBC_RELEASE_VERSION=...` and to both installer scripts
as `-Version ...`, so the ZIP name, the `.exe` version resource and the MSI
`ProductVersion` always agree. Locally the scripts read `CMakeLists.txt` when no version is
given.

The application's own version is generated too: `include/version.h.in` and
`src/ui/app.rc.in` are configured into `build/generated/`, so `--version`, the About box,
`v1.2.3` in the status bar and the `FILEVERSION`/`ProductVersion` resource all carry the
same number as the ZIP and the installers. Nothing is hardcoded twice.

## Releasing

### A patch/minor release (e.g. 1.0.1)

```powershell
# 1. bump the version local builds should use (optional but recommended)
#    project(DRBackupCreator VERSION 1.0.1)
git commit -am "Bump version to 1.0.1"

# 2. tag and push
git tag v1.0.1
git push origin master --tags
```

The workflow builds everything and creates the release **DRBackupCreator v1.0.1** with
release notes generated from the commits. Nothing else to do.

### 2.0 later

The pipeline is ready for it; a major release needs no workflow changes:

```powershell
# bump for local builds (optional — the tag alone decides the version CI stamps)
#   CMakeLists.txt: project(DRBackupCreator VERSION 2.0.0)
git commit -am "Bump version to 2.0.0"
git tag v2.0.0
git push origin master --tags
```

Result: a `v2.0.0` release whose assets are still `drbcWindowsInstaller.exe`,
`drbcWindowsInstaller.msi` and `DRBackupCreator-2.0.0-win32.zip`, so the website and the
`latest/download/...` links keep working without edits.

### Building a specific version without releasing

*Actions* → **build** → *Run workflow* → fill `version` (e.g. `2.0.0`) → run. The
installers come out stamped with that version as a downloadable artifact, and no release is
created.

## Static runtime (important)

MSYS2's GCC links `libstdc++-6.dll`, `libgcc_s_seh-1.dll` and `libwinpthread-1.dll`
dynamically by default. Shipping those in an installer is fragile, so `CMakeLists.txt`
passes `-static` to every MinGW target. The `windows` job also dumps the PE imports of
`drbc.exe` and `DRBackupCCore.dll` and **fails** if any MinGW runtime DLL shows up:

```text
drbc.exe -> COMCTL32.dll, GDI32.dll, KERNEL32.dll, msvcrt.dll, SHELL32.dll,
            USER32.dll, UxTheme.dll, DRBackupCCore.dll
DRBackupCCore.dll -> KERNEL32.dll, msvcrt.dll
```

If you add a dependency, check that list.

## Running the same steps locally

```powershell
./build.ps1 -Clean
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -B dist

./installer/build-installer.ps1 -NoBuild
./installer/build-msi.ps1 -NoBuild
```

The local build uses whatever MinGW GCC is on `PATH` (TDM-GCC here) with *MinGW Makefiles*;
CI uses Ninja. Both produce the same binaries.

## Troubleshooting

| Symptom | Cause / fix |
| --- | --- |
| `ISCC.exe not found` | Inno Setup 6 is preinstalled on the runner; the job falls back to `choco install innosetup -y`. |
| WiX download fails | `build-msi.ps1` fetches `wix` 6.0.2 from nuget.org into `%LOCALAPPDATA%\drbc-tools`; check the job's network step. |
| `MinGW runtime DLLs still linked` | Someone removed `-static` from `CMakeLists.txt`, or a new target was added without it. |
| Release job 403 | The `GITHUB_TOKEN` lacks write access. Org: *Settings → Actions → General → Workflow permissions* → **Read and write permissions**. |
| Tag push does not release | Tag must match `v*` (e.g. `v2.0.0`). Branch pushes only build. |
| Release asset missing | The build job failed before `Upload artifacts`, so `release` had nothing to download. |
| Build failed but the log needs a GitHub login | Every failed build copies its compiler errors to the run's **Annotations** and **Summary** tab (see `Report compile errors`), which are public. |
| ZIP named after the old version | CI overrides it with `-DDRBC_RELEASE_VERSION`; a local `cpack` uses `CMakeLists.txt`. |

## Notes

- Releases created by CI get notes from `generate_release_notes`; edit them afterwards if
  you want the custom `[STABLE] ...` style used for v1.0.0.
- The workflow does not sign the binaries. SmartScreen will still warn until a code-signing
  certificate is wired into the `windows` job.
