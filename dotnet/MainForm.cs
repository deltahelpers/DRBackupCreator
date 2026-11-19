using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace DRBackupCreator
{
    internal sealed class MainForm : Form
    {
        private readonly AppSettings settings;
        private readonly ListView slotList;
        private readonly Label status;
        private readonly Label location;
        private readonly Button themeButton;
        private readonly ToolStripMenuItem checkUpdatesMenu;
        private string saveDirectory;

        public MainForm(AppSettings appSettings)
        {
            settings = appSettings;
            Text = "DRBackupCreator";
            MinimumSize = new Size(780, 560);
            Size = new Size(980, 680);
            StartPosition = FormStartPosition.CenterScreen;
            KeyPreview = true;

            var root = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                ColumnCount = 1,
                RowCount = 5,
                Padding = new Padding(18)
            };
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            Controls.Add(root);

            var menu = new MenuStrip { Dock = DockStyle.Top };
            var fileMenu = new ToolStripMenuItem(UiText.Get(settings, "File", "Archivo"));
            fileMenu.DropDownItems.Add(UiText.Get(settings, "Import configuration...", "Importar configuración..."),
                null, (s, e) => ImportConfig());
            fileMenu.DropDownItems.Add(UiText.Get(settings, "Export configuration...", "Exportar configuración..."),
                null, (s, e) => ExportConfig());
            fileMenu.DropDownItems.Add(new ToolStripSeparator());
            fileMenu.DropDownItems.Add(UiText.Get(settings, "Exit", "Salir"), null, (s, e) => Close());
            var helpMenu = new ToolStripMenuItem(UiText.Get(settings, "Help", "Ayuda"));
            checkUpdatesMenu = new ToolStripMenuItem(UiText.Get(settings, "Check for updates", "Buscar actualizaciones"),
                null, async (s, e) => await CheckForUpdatesAsync(true));
            helpMenu.DropDownItems.Add(checkUpdatesMenu);
            helpMenu.DropDownItems.Add(UiText.Get(settings, "About", "Acerca de"), null, (s, e) => ShowAbout());
            menu.Items.Add(fileMenu);
            menu.Items.Add(helpMenu);
            MainMenuStrip = menu;
            root.Controls.Add(menu, 0, 0);

            var header = new TableLayoutPanel
            {
                Dock = DockStyle.Top,
                AutoSize = true,
                ColumnCount = 2,
                Padding = new Padding(0, 6, 0, 14)
            };
            header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            header.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            var title = new Label
            {
                Text = "DRBackupCreator",
                Font = new Font("Segoe UI", 21, FontStyle.Bold),
                AutoSize = true,
                Anchor = AnchorStyles.Left
            };
            var tagline = new Label
            {
                Text = UiText.Get(settings, "DELTARUNE save backup manager", "Administrador de backups de DELTARUNE"),
                AutoSize = true,
                ForeColor = Color.Gray,
                Padding = new Padding(2, 0, 0, 0)
            };
            var titleBox = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, AutoSize = true };
            titleBox.Controls.Add(title);
            titleBox.Controls.Add(tagline);
            themeButton = MakeButton("");
            themeButton.AutoSize = true;
            themeButton.Click += (s, e) => ToggleTheme();
            header.Controls.Add(titleBox, 0, 0);
            header.Controls.Add(themeButton, 1, 0);
            root.Controls.Add(header, 0, 1);

            var actions = new FlowLayoutPanel
            {
                Dock = DockStyle.Fill,
                AutoSize = true,
                WrapContents = true,
                Padding = new Padding(0, 0, 0, 12)
            };
            AddAction(actions, UiText.Get(settings, "Refresh", "Actualizar"), (s, e) => ReloadSlots());
            AddAction(actions, UiText.Get(settings, "Create backup", "Crear backup"), (s, e) => CreateBackup());
            AddAction(actions, UiText.Get(settings, "Backups", "Backups"), (s, e) => ShowBackups());
            AddAction(actions, UiText.Get(settings, "Open saves folder", "Abrir carpeta de partidas"),
                (s, e) => OpenFolder(saveDirectory));
            AddAction(actions, UiText.Get(settings, "Open backups folder", "Abrir carpeta de backups"),
                (s, e) => OpenFolder(SafeBackupDirectory()));
            AddAction(actions, UiText.Get(settings, "Settings", "Configuración"), (s, e) => OpenSettings());
            root.Controls.Add(actions, 0, 2);

            var listPanel = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                ColumnCount = 1,
                RowCount = 2
            };
            listPanel.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            listPanel.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            location = new Label { AutoSize = true, Padding = new Padding(0, 0, 0, 8) };
            slotList = new ListView
            {
                Dock = DockStyle.Fill,
                View = View.Details,
                FullRowSelect = true,
                MultiSelect = false,
                HideSelection = false,
                UseCompatibleStateImageBehavior = false
            };
            slotList.Columns.Add("#", 48);
            slotList.Columns.Add(UiText.Get(settings, "Chapter", "Capítulo"), 90);
            slotList.Columns.Add(UiText.Get(settings, "Slot", "Slot"), 90);
            slotList.Columns.Add(UiText.Get(settings, "Type", "Tipo"), 120);
            slotList.Columns.Add(UiText.Get(settings, "Size", "Tamaño"), 100);
            slotList.Columns.Add(UiText.Get(settings, "File", "Archivo"), 250);
            listPanel.Controls.Add(location, 0, 0);
            listPanel.Controls.Add(slotList, 0, 1);
            root.Controls.Add(listPanel, 0, 3);

            status = new Label { Dock = DockStyle.Fill, AutoSize = true, Padding = new Padding(0, 10, 0, 0) };
            root.Controls.Add(status, 0, 4);

            Load += async (s, e) =>
            {
                ReloadSlots();
                if (settings.UpdateAuto &&
                    DateTimeOffset.UtcNow.ToUnixTimeSeconds() - settings.UpdateLastCheck >= 86400)
                    await CheckForUpdatesAsync(false);
            };
            KeyDown += HandleShortcut;
            FormClosed += (s, e) => settings.Save();
            ThemeService.Apply(this, settings.Theme);
            UpdateThemeButton();
        }

        private void AddAction(FlowLayoutPanel panel, string caption, EventHandler handler)
        {
            var button = MakeButton(caption);
            button.Click += handler;
            panel.Controls.Add(button);
        }

        private static Button MakeButton(string caption)
        {
            return new Button
            {
                Text = caption,
                AutoSize = true,
                MinimumSize = new Size(90, 34),
                Padding = new Padding(10, 4, 10, 4),
                Margin = new Padding(0, 0, 8, 8),
                Cursor = Cursors.Hand
            };
        }

        private void ReloadSlots()
        {
            try
            {
                saveDirectory = BackupCore.GetSaveDirectory();
                slotList.Items.Clear();
                if (saveDirectory == null)
                {
                    location.Text = UiText.Get(settings, "Save folder not found: ", "No se encontró la carpeta de partidas: ") +
                                    Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "DELTARUNE");
                    status.Text = UiText.Get(settings, "Create a save in DELTARUNE, then refresh.", "Crea una partida y vuelve a actualizar.");
                    return;
                }

                var slots = BackupCore.ScanSlots(saveDirectory);
                BackupCore.GroupBySave(slots);
                location.Text = UiText.Get(settings, "Save folder: ", "Carpeta de partidas: ") + saveDirectory;
                for (int i = 0; i < slots.Count; i++)
                {
                    SlotFile slot = slots[i];
                    var item = new ListViewItem((i + 1).ToString());
                    item.SubItems.Add(slot.Chapter.ToString());
                    item.SubItems.Add(slot.Index.ToString());
                    item.SubItems.Add(slot.IsCompletion
                        ? UiText.Get(settings, "Completion", "Finalización")
                        : UiText.Get(settings, "Save", "Partida"));
                    item.SubItems.Add(BackupCore.FormatFileSize(slot.Size));
                    item.SubItems.Add(Path.GetFileName(slot.Path));
                    slotList.Items.Add(item);
                }
                status.Text = slots.Count == 0
                    ? UiText.Get(settings, "No save slots found.", "No se encontraron partidas.")
                    : UiText.Get(settings, slots.Count + " save slot(s) found.", slots.Count + " partida(s) encontrada(s).");
            }
            catch (Exception error)
            {
                ShowError(error);
            }
        }

        private void CreateBackup()
        {
            try
            {
                if (BackupCore.IsGameRunning())
                {
                    MessageBox.Show(this,
                        UiText.Get(settings, "Close DELTARUNE before creating a backup.",
                            "Cierra DELTARUNE antes de crear un backup."),
                        Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    return;
                }
                if (saveDirectory == null) ReloadSlots();
                if (saveDirectory == null) return;

                var slots = BackupCore.ScanSlots(saveDirectory);
                BackupCore.GroupBySave(slots);
                if (slots.Count == 0)
                {
                    MessageBox.Show(this, UiText.Get(settings, "No save slots were found.",
                        "No se encontraron partidas."), Text, MessageBoxButtons.OK, MessageBoxIcon.Information);
                    return;
                }

                string name = BackupCore.CreateBackup(settings.ResolveBackupDirectory(true), slots);
                status.Text = UiText.Get(settings, "Backup created: ", "Backup creado: ") + name;
                MessageBox.Show(this, UiText.Get(settings, "Backup created successfully:\n", "Backup creado correctamente:\n") + name,
                    Text, MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception error)
            {
                ShowError(error);
            }
        }

        private void ShowBackups()
        {
            try
            {
                string folder = settings.ResolveBackupDirectory(false);
                using (var dialog = new BackupDialog(settings,
                    BackupCore.ListAllBackups(folder), saveDirectory))
                {
                    if (dialog.ShowDialog(this) == DialogResult.OK)
                    {
                        status.Text = dialog.ResultMessage;
                        ReloadSlots();
                    }
                }
            }
            catch (Exception error)
            {
                ShowError(error);
            }
        }

        private void OpenSettings()
        {
            string previousLanguage = settings.Language;
            using (var dialog = new SettingsDialog(settings))
            {
                if (dialog.ShowDialog(this) == DialogResult.OK)
                {
                    if (previousLanguage != settings.Language)
                    {
                        Application.Restart();
                        Close();
                        return;
                    }
                    ThemeService.Apply(this, settings.Theme);
                    UpdateThemeButton();
                    ReloadSlots();
                }
            }
        }

        private void ToggleTheme()
        {
            settings.Theme = settings.Theme == "dark" ? "light" : "dark";
            settings.Save();
            ThemeService.Apply(this, settings.Theme);
            UpdateThemeButton();
        }

        private void UpdateThemeButton()
        {
            themeButton.Text = settings.Theme == "dark"
                ? UiText.Get(settings, "Light mode", "Modo claro")
                : UiText.Get(settings, "Dark mode", "Modo oscuro");
        }

        private string SafeBackupDirectory()
        {
            try { return settings.ResolveBackupDirectory(true); }
            catch (Exception error) { ShowError(error); return null; }
        }

        private void OpenFolder(string path)
        {
            if (String.IsNullOrWhiteSpace(path)) return;
            try
            {
                Directory.CreateDirectory(path);
                Process.Start(new ProcessStartInfo { FileName = "explorer.exe", Arguments = "\"" + path + "\"", UseShellExecute = true });
            }
            catch (Exception error) { ShowError(error); }
        }

        private void HandleShortcut(object sender, KeyEventArgs e)
        {
            if (e.KeyCode == Keys.F5) ReloadSlots();
            else if (e.Control && e.KeyCode == Keys.B) CreateBackup();
            else if (e.Control && e.KeyCode == Keys.L) ShowBackups();
            else if (e.Control && e.KeyCode == Keys.D) OpenFolder(saveDirectory);
            else return;
            e.Handled = true;
        }

        private void ImportConfig()
        {
            using (var picker = new OpenFileDialog
            {
                Filter = "DRBackupCreator config (*.bcconfig)|*.bcconfig|All files (*.*)|*.*",
                CheckFileExists = true
            })
            {
                if (picker.ShowDialog(this) != DialogResult.OK) return;
                try
                {
                    AppServices.ImportConfig(picker.FileName, settings);
                    ThemeService.Apply(this, settings.Theme);
                    UpdateThemeButton();
                    ReloadSlots();
                }
                catch (Exception error) { ShowError(error); }
            }
        }

        private void ExportConfig()
        {
            using (var picker = new SaveFileDialog
            {
                Filter = "DRBackupCreator config (*.bcconfig)|*.bcconfig",
                DefaultExt = "bcconfig",
                AddExtension = true
            })
            {
                if (picker.ShowDialog(this) != DialogResult.OK) return;
                try { AppServices.ExportConfig(picker.FileName, settings); }
                catch (Exception error) { ShowError(error); }
            }
        }

        private async Task CheckForUpdatesAsync(bool userInitiated)
        {
            checkUpdatesMenu.Enabled = false;
            try
            {
                ReleaseInfo release = await AppServices.GetLatestReleaseAsync();
                settings.UpdateLastCheck = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
                settings.Save();
                if (!AppServices.IsNewer(release.Version, AppServices.Version))
                {
                    if (userInitiated)
                        MessageBox.Show(this, UiText.Get(settings, "You are using the latest version.",
                            "Ya tienes la última versión."), Text, MessageBoxButtons.OK, MessageBoxIcon.Information);
                    return;
                }
                if (release.Version == settings.UpdateSkipped) return;

                var choice = MessageBox.Show(this,
                    UiText.Get(settings, "Version " + release.Version + " is available.\n\n" +
                        "Yes: install now. No: later. Cancel: skip this version.\n\n" + release.Notes,
                        "La versión " + release.Version + " está disponible.\n\nSí: instalar. No: más tarde. Cancelar: omitir esta versión.\n\n" + release.Notes),
                    Text, MessageBoxButtons.YesNoCancel, MessageBoxIcon.Information);
                if (choice == DialogResult.Cancel)
                {
                    settings.UpdateSkipped = release.Version;
                    settings.Save();
                    return;
                }
                if (choice != DialogResult.Yes) return;

                string installer = await AppServices.DownloadInstallerAsync(release, AppSettings.SettingsDirectory);
                Process.Start(new ProcessStartInfo { FileName = installer, UseShellExecute = true });
                Close();
            }
            catch (Exception error)
            {
                if (userInitiated) ShowError(error);
            }
            finally
            {
                if (!IsDisposed) checkUpdatesMenu.Enabled = true;
            }
        }

        private void ShowAbout()
        {
            MessageBox.Show(this, "DRBackupCreator " + AppServices.Version +
                "\nDELTARUNE save backup manager\n.NET Framework WinForms edition",
                Text, MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        private void ShowError(Exception error)
        {
            MessageBox.Show(this, error.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }
}
