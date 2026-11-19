# Configuration formats

DRBackupCreator accepts UTF-8 `.bcconfig` and `.bctheme` text files. Values use a small YAML-like syntax: one `key: value` per line, indented keys under a section, and `#` comments. Quote values containing `#` or `:`. Relative config paths resolve from the config file's directory.

## `.bcconfig`

```text
version: 1
language: en
theme_mode: dark
backup_dir: "%USERPROFILE%\Documents\DRBackups"
schedule:
  enabled: true
  time: "21:30"
```

Supported keys:

| Key | Values |
| --- | --- |
| `language` | `en` or `es` |
| `theme_mode` | `dark` or `light` |
| `theme` | Legacy `.bctheme` path; a background color selects dark or light mode |
| `backup_dir` | Backup directory; empty selects `%LOCALAPPDATA%\DRBackupCreator` |
| `schedule.enabled` | `true`, `false`, `1`, or `0` |
| `schedule.time` | 24-hour time such as `21:30` |

Unknown keys are ignored. Importing a config applies the recognized values and creates or removes the Windows scheduled task to match `schedule.enabled`.

## `.bctheme`

The WinForms version offers two standard-control palettes rather than custom owner-drawn themes. For legacy theme files, `colors.background` (or `background`) is read and its brightness selects the nearest light or dark palette. All other legacy theme properties are ignored.

## Settings and scheduling

Settings are stored in `%LOCALAPPDATA%\DRBackupCreator\settings.ini`; scheduled backup results append to `scheduled.log`. The daily task is named **DRBackupCreator Auto Backup** and runs `drbc.exe --auto-backup` while the signed-in user session is active.

```text
drbc.exe --config <file.bcconfig>
drbc.exe --export-config <file.bcconfig>
drbc.exe --theme <file.bctheme>
drbc.exe --schedule 21:30
drbc.exe --schedule off
drbc.exe --auto-backup
```

Double-clicking a `.bcconfig` or `.bctheme` file imports it and opens the app.
