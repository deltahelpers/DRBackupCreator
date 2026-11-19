using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace DRBackupCreator
{
    internal sealed class AppSettings
    {
        private const string Section = "app";

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern uint GetPrivateProfileString(
            string section, string key, string defaultValue, StringBuilder value,
            uint size, string fileName);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool WritePrivateProfileString(
            string section, string key, string value, string fileName);

        public string Theme { get; set; }
        public string Language { get; set; }
        public string BackupDirectory { get; set; }
        public bool ScheduleEnabled { get; set; }
        public string ScheduleTime { get; set; }
        public bool UpdateAuto { get; set; }
        public long UpdateLastCheck { get; set; }
        public string UpdateSkipped { get; set; }

        public static string SettingsDirectory
        {
            get
            {
                string local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
                if (String.IsNullOrWhiteSpace(local))
                    throw new InvalidOperationException("Windows did not provide a Local AppData path.");
                return Path.Combine(local, "DRBackupCreator");
            }
        }

        public static string SettingsPath
        {
            get { return Path.Combine(SettingsDirectory, "settings.ini"); }
        }

        public static AppSettings Load()
        {
            Directory.CreateDirectory(SettingsDirectory);
            string theme = Read(Section, "theme", "");
            if (theme != "dark" && theme != "light")
            {
                string legacyTheme = Read("theme", "path", "");
                theme = File.Exists(legacyTheme) ? AppServices.ThemeModeFromFile(legacyTheme) : "dark";
            }
            return new AppSettings
            {
                Theme = theme,
                Language = Read(Section, "language", Read("ui", "language", "es")),
                BackupDirectory = Read(Section, "backup_dir", Read("paths", "backup_dir", "")),
                ScheduleEnabled = Read(Section, "schedule_enabled", Read("schedule", "enabled", "0")) == "1",
                ScheduleTime = Read(Section, "schedule_time", Read("schedule", "time", "21:00")),
                UpdateAuto = Read(Section, "update_auto", Read("update", "auto", "1")) != "0",
                UpdateLastCheck = ParseLong(Read(Section, "update_last_check", Read("update", "last_check", "0"))),
                UpdateSkipped = Read(Section, "update_skipped", Read("update", "skipped", ""))
            };
        }

        public void Save()
        {
            Directory.CreateDirectory(SettingsDirectory);
            Write(Section, "theme", Theme);
            Write(Section, "language", Language);
            Write(Section, "backup_dir", BackupDirectory);
            Write(Section, "schedule_enabled", ScheduleEnabled ? "1" : "0");
            Write(Section, "schedule_time", ScheduleTime);
            Write(Section, "update_auto", UpdateAuto ? "1" : "0");
            Write(Section, "update_last_check", UpdateLastCheck.ToString());
            Write(Section, "update_skipped", UpdateSkipped);
        }

        public string ResolveBackupDirectory(bool create)
        {
            string path = String.IsNullOrWhiteSpace(BackupDirectory)
                ? SettingsDirectory
                : Environment.ExpandEnvironmentVariables(BackupDirectory.Trim());
            if (create) Directory.CreateDirectory(path);
            return path;
        }

        private static string Read(string section, string key, string fallback)
        {
            var value = new StringBuilder(2048);
            GetPrivateProfileString(section, key, fallback, value,
                (uint)value.Capacity, SettingsPath);
            return value.ToString();
        }

        private static void Write(string section, string key, string value)
        {
            if (!WritePrivateProfileString(section, key, value ?? String.Empty, SettingsPath))
                throw new IOException("Could not save setting '" + key + "' to " + SettingsPath);
        }

        private static long ParseLong(string value)
        {
            long result;
            return Int64.TryParse(value, out result) ? result : 0;
        }
    }
}
