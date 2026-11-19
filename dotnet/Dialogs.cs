using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace DRBackupCreator
{
    internal sealed class BackupDialog : Form
    {
        private readonly AppSettings settings;
        private readonly string savesDirectory;
        private readonly ListView backupsList;
        private List<BackupInfo> backups;

        public string ResultMessage { get; private set; }

        public BackupDialog(AppSettings appSettings, List<BackupInfo> items, string saveDirectory)
        {
            settings = appSettings;
            savesDirectory = saveDirectory;
            backups = items;
            Text = UiText.Get(settings, "Backups", "Backups");
            StartPosition = FormStartPosition.CenterParent;
            MinimumSize = new Size(680, 420);
            Size = new Size(780, 500);

            var layout = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(14),
                ColumnCount = 1,
                RowCount = 2
            };
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            layout.RowStyles.Add(new RowStyle(SizeType.AutoSize));

            backupsList = new ListView
            {
                Dock = DockStyle.Fill,
                View = View.Details,
                FullRowSelect = true,
                MultiSelect = false,
                UseCompatibleStateImageBehavior = false
            };
            backupsList.Columns.Add(UiText.Get(settings, "Date", "Fecha"), 210);
            backupsList.Columns.Add(UiText.Get(settings, "Slots", "Partidas"), 100);
            backupsList.Columns.Add(UiText.Get(settings, "Size", "Tamaño"), 110);
            backupsList.Columns.Add(UiText.Get(settings, "Folder", "Carpeta"), 300);
            backupsList.DoubleClick += (s, e) => RestoreSelected();
            layout.Controls.Add(backupsList, 0, 0);

            var buttons = new FlowLayoutPanel
            {
                Dock = DockStyle.Fill,
                FlowDirection = FlowDirection.RightToLeft,
                AutoSize = true,
                Padding = new Padding(0, 12, 0, 0)
            };
            AddButton(buttons, UiText.Get(settings, "Close", "Cerrar"), (s, e) => Close());
            AddButton(buttons, UiText.Get(settings, "Delete", "Eliminar"), (s, e) => DeleteSelected());
            AddButton(buttons, UiText.Get(settings, "Restore", "Restaurar"), (s, e) => RestoreSelected());
            layout.Controls.Add(buttons, 0, 1);
            Controls.Add(layout);

            RefreshList();
            ThemeService.Apply(this, settings.Theme);
        }

        private void AddButton(FlowLayoutPanel panel, string caption, EventHandler handler)
        {
            var button = new Button
            {
                Text = caption,
                AutoSize = true,
                MinimumSize = new Size(90, 34),
                Margin = new Padding(6, 0, 0, 0)
            };
            button.Click += handler;
            panel.Controls.Add(button);
        }

        private void RefreshList()
        {
            backupsList.Items.Clear();
            foreach (BackupInfo backup in backups)
            {
                var row = new ListViewItem(backup.Timestamp.Replace('_', ' '));
                row.SubItems.Add(backup.SlotCount.ToString());
                row.SubItems.Add(BackupCore.FormatFileSize(backup.TotalSize));
                row.SubItems.Add(Path.GetFileName(backup.Path));
                row.Tag = backup;
                backupsList.Items.Add(row);
            }
        }

        private BackupInfo Selected()
        {
            return backupsList.SelectedItems.Count == 0 ? null : backupsList.SelectedItems[0].Tag as BackupInfo;
        }

        private void RestoreSelected()
        {
            BackupInfo backup = Selected();
            if (backup == null) return;
            if (String.IsNullOrWhiteSpace(savesDirectory))
            {
                MessageBox.Show(this, UiText.Get(settings, "The DELTARUNE save folder was not found.",
                    "No se encontró la carpeta de partidas de DELTARUNE."), Text,
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }
            if (BackupCore.IsGameRunning())
            {
                MessageBox.Show(this, UiText.Get(settings, "Close DELTARUNE before restoring a backup.",
                    "Cierra DELTARUNE antes de restaurar un backup."), Text,
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            string prompt = UiText.Get(settings,
                "Restore this backup? Existing save files will be overwritten.",
                "¿Restaurar este backup? Los archivos existentes se sobrescribirán.");
            if (MessageBox.Show(this, prompt, Text, MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes)
                return;

            try
            {
                int count = BackupCore.RestoreBackup(backup.Path, savesDirectory);
                ResultMessage = UiText.Get(settings, "Restored " + count + " file(s).",
                    "Se restauraron " + count + " archivo(s).");
                MessageBox.Show(this, ResultMessage, Text, MessageBoxButtons.OK, MessageBoxIcon.Information);
                DialogResult = DialogResult.OK;
                Close();
            }
            catch (Exception error)
            {
                MessageBox.Show(this, error.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void DeleteSelected()
        {
            BackupInfo backup = Selected();
            if (backup == null) return;
            if (MessageBox.Show(this,
                UiText.Get(settings, "Move this backup to the Recycle Bin?", "¿Mover este backup a la Papelera?"),
                Text, MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;
            try
            {
                BackupCore.DeleteBackup(backup.Path);
                backups.Remove(backup);
                RefreshList();
                ResultMessage = UiText.Get(settings, "Backup deleted.", "Backup eliminado.");
            }
            catch (Exception error)
            {
                MessageBox.Show(this, error.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }

    internal sealed class SettingsDialog : Form
    {
        private readonly AppSettings settings;
        private readonly ComboBox theme;
        private readonly ComboBox language;
        private readonly TextBox backupFolder;
        private readonly CheckBox scheduleEnabled;
        private readonly MaskedTextBox scheduleTime;
        private readonly CheckBox updateAuto;

        public SettingsDialog(AppSettings appSettings)
        {
            settings = appSettings;
            Text = UiText.Get(settings, "Settings", "Configuración");
            StartPosition = FormStartPosition.CenterParent;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            ShowInTaskbar = false;
            AutoSize = true;
            AutoSizeMode = AutoSizeMode.GrowAndShrink;

            var layout = new TableLayoutPanel
            {
                AutoSize = true,
                Padding = new Padding(18),
                ColumnCount = 3,
                RowCount = 7
            };
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));

            theme = AddCombo(layout, 0, UiText.Get(settings, "Appearance", "Apariencia"),
                new[] { "Dark", "Light" });
            theme.SelectedIndex = settings.Theme == "light" ? 1 : 0;
            language = AddCombo(layout, 1, UiText.Get(settings, "Language", "Idioma"),
                new[] { "English", "Español" });
            language.SelectedIndex = settings.Language == "es" ? 1 : 0;

            AddLabel(layout, 2, UiText.Get(settings, "Backup folder", "Carpeta de backups"));
            backupFolder = new TextBox { Dock = DockStyle.Fill, Text = settings.BackupDirectory ?? "" };
            layout.Controls.Add(backupFolder, 1, 2);
            var browse = new Button { Text = UiText.Get(settings, "Browse...", "Buscar..."), AutoSize = true };
            browse.Click += (s, e) =>
            {
                using (var picker = new FolderBrowserDialog())
                {
                    if (Directory.Exists(backupFolder.Text)) picker.SelectedPath = backupFolder.Text;
                    if (picker.ShowDialog(this) == DialogResult.OK) backupFolder.Text = picker.SelectedPath;
                }
            };
            layout.Controls.Add(browse, 2, 2);

            scheduleEnabled = new CheckBox
            {
                Text = UiText.Get(settings, "Daily backup", "Backup diario"),
                AutoSize = true,
                Checked = settings.ScheduleEnabled,
                Anchor = AnchorStyles.Left
            };
            layout.Controls.Add(scheduleEnabled, 0, 3);
            scheduleTime = new MaskedTextBox { Mask = "00:00", Text = settings.ScheduleTime, Width = 70 };
            layout.Controls.Add(scheduleTime, 1, 3);
            var scheduleHint = new Label
            {
                Text = UiText.Get(settings, "24-hour time", "Formato 24 horas"),
                AutoSize = true,
                Anchor = AnchorStyles.Left
            };
            layout.Controls.Add(scheduleHint, 2, 3);

            updateAuto = new CheckBox
            {
                Text = UiText.Get(settings, "Check for updates once a day", "Buscar actualizaciones una vez al día"),
                AutoSize = true,
                Checked = settings.UpdateAuto,
                Anchor = AnchorStyles.Left
            };
            layout.Controls.Add(updateAuto, 0, 4);
            layout.SetColumnSpan(updateAuto, 3);

            var buttons = new FlowLayoutPanel
            {
                AutoSize = true,
                FlowDirection = FlowDirection.RightToLeft,
                Dock = DockStyle.Fill,
                Padding = new Padding(0, 14, 0, 0)
            };
            var save = new Button { Text = UiText.Get(settings, "Save", "Guardar"), AutoSize = true, DialogResult = DialogResult.None };
            save.Click += SaveSettings;
            var cancel = new Button { Text = UiText.Get(settings, "Cancel", "Cancelar"), AutoSize = true, DialogResult = DialogResult.Cancel };
            buttons.Controls.Add(cancel);
            buttons.Controls.Add(save);
            layout.Controls.Add(buttons, 0, 6);
            layout.SetColumnSpan(buttons, 3);
            Controls.Add(layout);
            AcceptButton = save;
            CancelButton = cancel;
            ThemeService.Apply(this, settings.Theme);
        }

        private ComboBox AddCombo(TableLayoutPanel layout, int row, string label, string[] options)
        {
            AddLabel(layout, row, label);
            var combo = new ComboBox
            {
                DropDownStyle = ComboBoxStyle.DropDownList,
                Width = 180
            };
            combo.Items.AddRange(options);
            layout.Controls.Add(combo, 1, row);
            layout.SetColumnSpan(combo, 2);
            return combo;
        }

        private static void AddLabel(TableLayoutPanel layout, int row, string text)
        {
            layout.Controls.Add(new Label { Text = text, AutoSize = true, Anchor = AnchorStyles.Left, Margin = new Padding(3, 10, 12, 10) }, 0, row);
        }

        private void SaveSettings(object sender, EventArgs e)
        {
            string time = scheduleTime.Text;
            if (scheduleEnabled.Checked && !DateTime.TryParseExact(time, "HH:mm",
                System.Globalization.CultureInfo.InvariantCulture,
                System.Globalization.DateTimeStyles.None, out _))
            {
                MessageBox.Show(this, UiText.Get(settings, "Enter a valid time between 00:00 and 23:59.",
                    "Escribe una hora válida entre 00:00 y 23:59."), Text,
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            string error;
            if (!AppServices.SetSchedule(scheduleEnabled.Checked, time, out error))
            {
                MessageBox.Show(this, error, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            settings.Theme = theme.SelectedIndex == 1 ? "light" : "dark";
            settings.Language = language.SelectedIndex == 1 ? "es" : "en";
            settings.BackupDirectory = backupFolder.Text.Trim();
            settings.ScheduleEnabled = scheduleEnabled.Checked;
            settings.ScheduleTime = time;
            settings.UpdateAuto = updateAuto.Checked;
            try
            {
                settings.Save();
                DialogResult = DialogResult.OK;
                Close();
            }
            catch (Exception errorSave)
            {
                MessageBox.Show(this, errorSave.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
    }
}
