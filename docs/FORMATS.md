# DRBackupCreator file formats

DRBackupCreator 2.0 understands two small text files:

| File | What it controls | Opens with |
| --- | --- | --- |
| `.bctheme` | colours, fonts and window size | double-click, or **Settings → Theme → Load…** |
| `.bcconfig` | language, theme, backup folder and daily schedule | double-click, or **Settings → Configuration → Import…** |

Both use the same syntax. You can write them in Notepad: they are plain UTF-8
text.

---

## 1. Syntax

```text
# Lines starting with '#' are comments and are ignored.
name: Midnight Blue            # a comment can also go after a value
colors:                        # a line ending in ':' starts a section
  accent: "#4d8bf5"            # keys inside the section are indented
  panel: '#252526'             # single or double quotes are both fine
```

Rules:

- One `key: value` per line.
- Sections are indented with **2 spaces** (tabs also work).
- Quotes are optional, but use them when the value contains `#` or `:`.
- Unknown keys are ignored, so a file made for a newer version still loads.
- If the same key appears twice, the last one wins.
- `//` is **not** a comment. Use `#`.

If a file is wrong, the app tells you the line number, for example:

```text
line 7: "#12345" is not a colour (use #rrggbb)
```

---

## 2. `.bctheme`

### 2.1 Example

```text
# Midnight Blue
name: Midnight Blue
version: 1

colors:
  background:       "#101820"
  background_deep:  "#0b121a"
  panel:            "#16222d"
  panel_alt:        "#1c2a37"
  input:            "#0d151c"
  border:           "#263644"
  border_hot:       "#3a5064"
  text:             "#e6eef5"
  muted:            "#93a4b3"
  accent:           "#4d8bf5"
  accent_hot:       "#6ba1ff"
  success:          "#7fd18f"
  danger:           "#f07c86"
  selection:        "#1d3550"
  warning_bg:       "#3a1f24"
  warning_line:     "#7a3a42"

font:
  family: "Consolas"
  scale: 110

window:
  width: 1100
  height: 820
  min_width: 900
  min_height: 700
```

### 2.2 Keys

**General**

| Key | Meaning | Default |
| --- | --- | --- |
| `name` | Name shown in **Settings → Theme** | `Default` |
| `version` | Format version, currently `1` | — |

**colors** — every value is `#rrggbb` or the short `#rgb` form.

| Key | Where it is used |
| --- | --- |
| `background` | main window background |
| `background_deep` | darker areas (headers, separators) |
| `panel` | cards and grouped boxes |
| `panel_alt` | buttons and secondary panels |
| `input` | text fields and the slot list |
| `border` | normal outline |
| `border_hot` | outline under the mouse |
| `text` | normal text |
| `muted` | hints, subtitles, secondary text |
| `accent` | main colour (title bar stripe, primary button) |
| `accent_hot` | accent under the mouse |
| `success` | "backup created" messages |
| `danger` | errors |
| `selection` | selected row in the list |
| `warning_bg` | background of the "game is running" banner |
| `warning_line` | outline of that banner |

**font**

| Key | Meaning | Range | Default |
| --- | --- | --- | --- |
| `family` | Any font installed in Windows, e.g. `"Segoe UI"`, `"Consolas"` | — | `Segoe UI` |
| `scale` | Text size in percent | 50–200 | `100` |

**window**

| Key | Meaning | Range | Default |
| --- | --- | --- | --- |
| `width` | Window width when the app starts | 640–4000 | `960` |
| `height` | Window height when the app starts | 520–4000 | `740` |
| `min_width` | Smallest width the user can resize to | 500–4000 | `800` |
| `min_height` | Smallest height the user can resize to | 420–4000 | `620` |

Notes:

- Sizes are in DIPs (the same unit Windows uses for display scaling), so the
  window looks the same at 100%, 125% or 150%.
- `min_width`/`min_height` are reduced automatically if they are bigger than
  `width`/`height`.
- The size is read when the app starts. Changing the file while the app is open
  updates the colours immediately, but the window size only after a restart.
- If the requested window is taller or wider than your screen, the app shrinks
  it to fit.

### 2.3 Where the app keeps the chosen theme

```
%LOCALAPPDATA%\DRBackupCreator\settings.ini

[theme]
path=C:\Users\You\Documents\mytheme.bctheme
```

Delete the `path` line (or press **Default** in Settings) to go back to the
built-in dark theme.

---

## 3. `.bcconfig`

A `.bcconfig` is a **complete** configuration. Importing one applies every
value it contains, so you can use it to set up another PC in one step.

### 3.1 Example

```text
# My setup
version: 1

# "es" (Spanish) or "en" (English)
language: en

# Theme file. "default" = built-in theme.
# Relative paths are relative to THIS file, so you can ship a folder with
# the config and the theme together.
theme: "midnight.bctheme"

# Where backups go. Empty = %LOCALAPPDATA%\DRBackupCreator
# %USERPROFILE%, %ONEDRIVE% and any other environment variable work.
backup_dir: "%USERPROFILE%\Documents\DRBackups"

schedule:
  enabled: true
  time: "21:30"
```

### 3.2 Keys

| Key | Meaning | Values |
| --- | --- | --- |
| `language` | Interface language | `es`, `en` |
| `theme` | Theme file to use | a path, or `default` |
| `backup_dir` | Folder for new backups | a path; empty = default |
| `schedule.enabled` | Daily backup on/off | `true`, `false`, `yes`, `no`, `on`, `off`, `1`, `0` |
| `schedule.time` | Time of the daily backup | `21:30`, `9:05`, `2130`, `21.30` |

Notes:

- `theme` and `backup_dir` may be relative. They are resolved against the
  folder that contains the `.bcconfig` file.
- Importing a config **replaces** the current settings; it does not merge.
- Turning the schedule on creates the Windows task immediately. Turning it off
  deletes it.
- Export the current settings with **Settings → Configuration → Export…** or
  `drbc.exe --export-config my.bcconfig`. The exported file is a good starting
  point.

### 3.3 Where the app keeps the schedule

```
%LOCALAPPDATA%\DRBackupCreator\settings.ini

[schedule]
enabled=1
time=21:30
```

The actual job lives in Windows Task Scheduler as
**DRBackupCreator Auto Backup**. It runs `drbc.exe --auto-backup`, which
creates a backup and writes one line to
`%LOCALAPPDATA%\DRBackupCreator\scheduled.log`:

```text
2026-11-18 21:30:02 ok - backup_2026-11-18_21-30-02 (5 slots)
2026-11-19 21:30:01 skipped - DELTARUNE is running
```

---

## 4. Command line

```text
drbc.exe --theme <file.bctheme>     Load and remember a theme
drbc.exe --config <file.bcconfig>   Import a configuration
drbc.exe --export-config <file>     Write the current settings
drbc.exe --schedule 21:30           Create the daily backup task
drbc.exe --schedule off             Remove it
drbc.exe --auto-backup              What the task runs (no window)
```

Double-clicking a `.bctheme` or `.bcconfig` does the same thing from Explorer:
the file is applied and the main window opens.

---

## 5. Common mistakes

| Message | Fix |
| --- | --- |
| `expected "key: value"` | The line has no `:`. Comments use `#`, not `//`. |
| `unterminated quoted value` | A value starts with `"` or `'` but never closes. |
| `"red" is not a colour` | Use `#rrggbb` (6 digits) or `#rgb` (3 digits). |
| `font.scale must be a number from 50 to 200` | Write `110`, not `110%`. |
| `lists are not supported` | Do not use `- item` lines; use one `key: value` per line. |
| `No se pudo cargar el tema` / `Could not load the theme` | The `theme:` path in the config is wrong or the file is invalid. |

---

## 6. Resumen en español

- Los dos archivos son texto normal: `clave: valor`, con secciones indentadas
  y comentarios que empiezan por `#`.
- `.bctheme` cambia **colores** (`#rrggbb`), **fuente** (`family`, `scale`
  50–200) y **tamaño de ventana** (`width`, `height`, `min_width`,
  `min_height`).
- `.bcconfig` cambia **idioma**, **tema**, **carpeta de backups** y el
  **backup diario** (`schedule.enabled`, `schedule.time`).
- En un `.bcconfig` las rutas relativas se resuelven respecto al propio
  archivo, así que puedes repartir una carpeta con el `.bcconfig` y el
  `.bctheme` juntos.
- El backup diario usa el **Programador de tareas de Windows**
  (`DRBackupCreator Auto Backup`) y funciona con la app cerrada, siempre que tu
  usuario tenga la sesión iniciada.
- Si algo está mal escrito, el mensaje dice el número de línea.
