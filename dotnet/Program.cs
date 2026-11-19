using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace DRBackupCreator
{
    internal static class Program
    {
        [DllImport("kernel32.dll")]
        private static extern bool FreeConsole();

        [STAThread]
        private static int Main(string[] args)
        {
            Console.OutputEncoding = new System.Text.UTF8Encoding(false);
            try
            {
                if (Has(args, "--auto-backup"))
                {
                    FreeConsole();
                    return AppServices.RunAutoBackup();
                }
                if (Has(args, "--help") || Has(args, "-h"))
                {
                    PrintHelp();
                    return 0;
                }
                if (Has(args, "--version") || Has(args, "-v"))
                {
                    Console.WriteLine("DRBackupCreator " + AppServices.Version);
                    return 0;
                }
                if (Has(args, "--cli") || Has(args, "-c")) return RunCli(args);
                if (TryArgument(args, "--schedule", out var schedule))
                {
                    return RunSchedule(schedule);
                }
                if (TryArgument(args, "--theme", out var themeFile))
                {
                    var settings = AppSettings.Load();
                    AppServices.ImportTheme(themeFile, settings);
                    return 0;
                }
                if (TryArgument(args, "--config", out var configFile))
                {
                    AppServices.ImportConfig(configFile, AppSettings.Load());
                    return 0;
                }
                if (TryArgument(args, "--export-config", out var exportFile))
                {
                    AppServices.ExportConfig(exportFile, AppSettings.Load());
                    return 0;
                }
                if (Has(args, "--check-updates"))
                    return CheckUpdates().GetAwaiter().GetResult();
                if (TryArgument(args, "--download-update", out var downloadDirectory))
                    return DownloadUpdate(downloadDirectory).GetAwaiter().GetResult();

                AppSettings appSettings = AppSettings.Load();
                if (args.Length > 0 && !args[0].StartsWith("-", StringComparison.Ordinal))
                {
                    string extension = Path.GetExtension(args[0]);
                    if (String.Equals(extension, ".bcconfig", StringComparison.OrdinalIgnoreCase))
                        AppServices.ImportConfig(args[0], appSettings);
                    else if (String.Equals(extension, ".bctheme", StringComparison.OrdinalIgnoreCase))
                        AppServices.ImportTheme(args[0], appSettings);
                }

                FreeConsole();
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new MainForm(appSettings));
                return 0;
            }
            catch (Exception error)
            {
                if (Environment.UserInteractive && !Has(args, "--cli") && !Has(args, "--auto-backup") &&
                    !Has(args, "--schedule") && !Has(args, "--check-updates") &&
                    !Has(args, "--download-update") && !Has(args, "--config") &&
                    !Has(args, "--export-config") && !Has(args, "--theme"))
                {
                    MessageBox.Show(error.Message, "DRBackupCreator", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return 1;
                }
                Console.Error.WriteLine(error.Message);
                return 1;
            }
        }

        private static int RunCli(string[] args)
        {
            AppSettings settings = AppSettings.Load();
            string saveDirectory = BackupCore.GetSaveDirectory();
            if (saveDirectory == null)
            {
                Console.Error.WriteLine("The DELTARUNE save folder was not found.");
                return 1;
            }
            if (BackupCore.IsGameRunning())
                Console.WriteLine("Warning: DELTARUNE is running.");
            var slots = BackupCore.ScanSlots(saveDirectory);
            BackupCore.GroupBySave(slots);

            int commandIndex = Array.FindIndex(args, item => item == "--cli" || item == "-c") + 1;
            string command = commandIndex < args.Length ? args[commandIndex] : "";
            if (command == "list")
            {
                Console.WriteLine("Chapter\tSlot\tType\tSize\tFile");
                foreach (SlotFile slot in slots)
                    Console.WriteLine(slot.Chapter + "\t" + slot.Index + "\t" +
                        (slot.IsCompletion ? "Completion" : "Save") + "\t" +
                        BackupCore.FormatFileSize(slot.Size) + "\t" + Path.GetFileName(slot.Path));
                return 0;
            }
            if (command == "backup")
            {
                if (BackupCore.IsGameRunning())
                {
                    Console.Error.WriteLine("Close DELTARUNE before creating a backup.");
                    return 1;
                }
                string name = BackupCore.CreateBackup(settings.ResolveBackupDirectory(true), slots);
                if (name == null) throw new InvalidOperationException("No save slots were found.");
                Console.WriteLine("Backup created: " + name);
                return 0;
            }
            if (command == "restore")
            {
                var backups = BackupCore.ListAllBackups(settings.ResolveBackupDirectory(false));
                int index;
                if (commandIndex + 1 >= args.Length || !Int32.TryParse(args[commandIndex + 1], out index) ||
                    index < 0 || index >= backups.Count)
                    throw new ArgumentException("Usage: drbc.exe --cli restore <index>");
                if (BackupCore.IsGameRunning())
                {
                    Console.Error.WriteLine("Close DELTARUNE before restoring a backup.");
                    return 1;
                }
                int count = BackupCore.RestoreBackup(backups[index].Path, saveDirectory);
                Console.WriteLine("Restored " + count + " file(s).");
                return 0;
            }

            PrintHelp();
            return 0;
        }

        private static int RunSchedule(string value)
        {
            AppSettings settings = AppSettings.Load();
            bool enabled = !String.Equals(value, "off", StringComparison.OrdinalIgnoreCase) &&
                           !String.Equals(value, "disable", StringComparison.OrdinalIgnoreCase);
            string time = enabled ? value : settings.ScheduleTime;
            string error;
            if (!AppServices.SetSchedule(enabled, time, out error))
            {
                Console.Error.WriteLine(error);
                return 1;
            }
            settings.ScheduleEnabled = enabled;
            settings.ScheduleTime = time;
            settings.Save();
            Console.WriteLine(enabled ? "Daily backup scheduled at " + time : "Daily backup disabled.");
            return 0;
        }

        private static async Task<int> CheckUpdates()
        {
            ReleaseInfo release = await AppServices.GetLatestReleaseAsync();
            if (AppServices.IsNewer(release.Version, AppServices.Version))
            {
                Console.WriteLine(release.Version + " available (you have " + AppServices.Version + ")");
                if (!String.IsNullOrEmpty(release.Url)) Console.WriteLine(release.Url);
            }
            else Console.WriteLine("up to date (" + AppServices.Version + ")");
            return 0;
        }

        private static async Task<int> DownloadUpdate(string directory)
        {
            ReleaseInfo release = await AppServices.GetLatestReleaseAsync();
            string file = await AppServices.DownloadInstallerAsync(release, directory);
            Console.WriteLine("downloaded " + release.Version + " -> " + file);
            return 0;
        }

        private static bool TryArgument(string[] args, string name, out string value)
        {
            int index = Array.FindIndex(args, item => String.Equals(item, name, StringComparison.OrdinalIgnoreCase));
            if (index >= 0 && index + 1 < args.Length)
            {
                value = args[index + 1];
                return true;
            }
            value = null;
            return false;
        }

        private static bool Has(string[] args, string name)
        {
            return Array.Exists(args, item => String.Equals(item, name, StringComparison.OrdinalIgnoreCase));
        }

        private static void PrintHelp()
        {
            Console.WriteLine(
                "DRBackupCreator " + AppServices.Version + "\n\n" +
                "Usage:\n" +
                "  drbc.exe                          Opens the GUI\n" +
                "  drbc.exe --cli list               Lists detected save slots\n" +
                "  drbc.exe --cli backup             Creates a backup\n" +
                "  drbc.exe --cli restore <index>    Restores a backup\n" +
                "  drbc.exe --schedule 21:30         Schedules a daily backup\n" +
                "  drbc.exe --schedule off           Disables the daily backup\n" +
                "  drbc.exe --auto-backup            Runs a scheduled backup\n" +
                "  drbc.exe --config <file>          Imports settings\n" +
                "  drbc.exe --export-config <file>   Exports settings\n" +
                "  drbc.exe --theme <file>           Selects dark/light from a theme\n" +
                "  drbc.exe --check-updates          Checks GitHub for a newer release\n" +
                "  drbc.exe --download-update <dir>  Downloads and verifies the installer\n" +
                "  drbc.exe --help                   Prints this help\n" +
                "  drbc.exe --version                Prints the version");
        }
    }
}
