using System;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace DRBackupCreator
{
    internal static class AppServices
    {
        private const string TaskName = "DRBackupCreator Auto Backup";

        public static string Version
        {
            get { return typeof(AppServices).Assembly.GetName().Version.ToString(3); }
        }

        public static bool SetSchedule(bool enabled, string time, out string error)
        {
            error = null;
            if (enabled && !IsClock(time))
            {
                error = "The schedule time must be in HH:mm format.";
                return false;
            }

            string arguments;
            if (enabled)
            {
                string exe = Process.GetCurrentProcess().MainModule.FileName;
                string action = "\\\"" + exe + "\\\" --auto-backup";
                arguments = "/Create /F /TN \"" + TaskName + "\" /TR \"" + action +
                            "\" /SC DAILY /ST " + time + " /RL LIMITED";
            }
            else
            {
                string queryOutput;
                int queryExitCode;
                if (!RunTaskCommand("/Query /TN \"" + TaskName + "\"", out queryOutput, out queryExitCode) ||
                    queryExitCode != 0)
                    return true;
                arguments = "/Delete /F /TN \"" + TaskName + "\"";
            }

            string output;
            int exitCode;
            if (!RunTaskCommand(arguments, out output, out exitCode))
            {
                error = "Could not start Windows Task Scheduler.";
                return false;
            }
            if (exitCode != 0)
            {
                error = String.IsNullOrWhiteSpace(output)
                    ? "Windows Task Scheduler rejected the request."
                    : output.Trim();
                return false;
            }
            return true;
        }

        public static int RunAutoBackup()
        {
            string result;
            string log = Path.Combine(AppSettings.SettingsDirectory, "scheduled.log");
            Directory.CreateDirectory(AppSettings.SettingsDirectory);
            try
            {
                if (BackupCore.IsGameRunning())
                {
                    result = "skipped - DELTARUNE is running";
                    AppendLog(log, result);
                    return 0;
                }

                string saves = BackupCore.GetSaveDirectory();
                if (saves == null)
                {
                    result = "skipped - no DELTARUNE folder";
                    AppendLog(log, result);
                    return 1;
                }

                var slots = BackupCore.ScanSlots(saves);
                BackupCore.GroupBySave(slots);
                if (slots.Count == 0)
                {
                    result = "skipped - no save slots found";
                    AppendLog(log, result);
                    return 1;
                }

                string name = BackupCore.CreateBackup(AppSettings.Load().ResolveBackupDirectory(true), slots);
                result = "ok - " + name + " (" + slots.Count + " slots)";
                AppendLog(log, result);
                return 0;
            }
            catch (Exception error)
            {
                AppendLog(log, "failed - " + error.Message);
                return 1;
            }
        }

        public static void ImportConfig(string file, AppSettings settings)
        {
            if (!File.Exists(file)) throw new FileNotFoundException("Configuration file not found.", file);
            string baseDirectory = Path.GetDirectoryName(Path.GetFullPath(file));
            var values = new System.Collections.Generic.Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var sections = new System.Collections.Generic.List<Tuple<int, string>>();

            int lineNumber = 0;
            foreach (string original in File.ReadAllLines(file, Encoding.UTF8))
            {
                lineNumber++;
                string line = StripComment(original);
                if (String.IsNullOrWhiteSpace(line)) continue;
                int indent = line.Length - line.TrimStart(' ', '\t').Length;
                string body = line.Trim();
                int colon = body.IndexOf(':');
                if (colon < 1)
                    throw new FormatException("Invalid configuration at line " + lineNumber + ".");
                string key = body.Substring(0, colon).Trim();
                string value = Unquote(body.Substring(colon + 1).Trim());
                if (value.Length == 0)
                {
                    while (sections.Count > 0 && sections[sections.Count - 1].Item1 >= indent)
                        sections.RemoveAt(sections.Count - 1);
                    sections.Add(Tuple.Create(indent, key));
                    continue;
                }
                while (sections.Count > 0 && sections[sections.Count - 1].Item1 >= indent)
                    sections.RemoveAt(sections.Count - 1);
                string full = String.Join(".", sections.ConvertAll(item => item.Item2).ToArray());
                values[(full.Length == 0 ? "" : full + ".") + key] = value;
            }

            string language, mode, backupDirectory, enabled, time;
            if (values.TryGetValue("language", out language)) settings.Language = language == "es" ? "es" : "en";
            if (values.TryGetValue("theme_mode", out mode)) settings.Theme = mode == "light" ? "light" : "dark";
            else if (values.TryGetValue("theme", out mode) &&
                     !String.Equals(mode, "default", StringComparison.OrdinalIgnoreCase))
                settings.Theme = ThemeModeFromFile(ResolveRelative(baseDirectory, mode));
            if (values.TryGetValue("backup_dir", out backupDirectory))
                settings.BackupDirectory = ResolveRelative(baseDirectory, backupDirectory);
            if (values.TryGetValue("schedule.enabled", out enabled))
                settings.ScheduleEnabled = enabled.Equals("true", StringComparison.OrdinalIgnoreCase) || enabled == "1";
            if (values.TryGetValue("schedule.time", out time) && IsClock(time))
                settings.ScheduleTime = time;

            settings.Save();
            string scheduleError;
            if (!SetSchedule(settings.ScheduleEnabled, settings.ScheduleTime, out scheduleError))
                throw new InvalidOperationException(scheduleError);
        }

        public static void ExportConfig(string file, AppSettings settings)
        {
            string backup = settings.BackupDirectory ?? "";
            string contents = "version: 1\n" +
                "language: " + settings.Language + "\n" +
                "theme_mode: " + settings.Theme + "\n" +
                "backup_dir: \"" + Escape(backup) + "\"\n" +
                "schedule:\n" +
                "  enabled: " + (settings.ScheduleEnabled ? "true" : "false") + "\n" +
                "  time: \"" + Escape(settings.ScheduleTime) + "\"\n";
            File.WriteAllText(file, contents, new UTF8Encoding(true));
        }

        public static void ImportTheme(string file, AppSettings settings)
        {
            if (!File.Exists(file)) throw new FileNotFoundException("Theme file not found.", file);
            settings.Theme = ThemeModeFromFile(file);
            settings.Save();
        }

        public static string ThemeModeFromFile(string file)
        {
            if (!File.Exists(file)) return "dark";
            string text = File.ReadAllText(file, Encoding.UTF8);
            string background = FindFormatValue(text, "background");
            if (background == null) background = FindFormatValue(text, "bg");
            if (background != null)
            {
                string hex = background.Trim().TrimStart('#');
                if (hex.Length == 3)
                    hex = new String(new[] { hex[0], hex[0], hex[1], hex[1], hex[2], hex[2] });
                if (hex.Length == 6)
                {
                    int r, g, b;
                    if (Int32.TryParse(hex.Substring(0, 2), System.Globalization.NumberStyles.HexNumber, null, out r) &&
                        Int32.TryParse(hex.Substring(2, 2), System.Globalization.NumberStyles.HexNumber, null, out g) &&
                        Int32.TryParse(hex.Substring(4, 2), System.Globalization.NumberStyles.HexNumber, null, out b))
                        return (r * 299 + g * 587 + b * 114) / 1000 < 128 ? "dark" : "light";
                }
            }
            return "dark";
        }

        public static async Task<ReleaseInfo> GetLatestReleaseAsync()
        {
            using (var client = new HttpClient())
            {
                client.Timeout = TimeSpan.FromSeconds(20);
                client.DefaultRequestHeaders.UserAgent.ParseAdd("DRBackupCreator/" + Version);
                client.DefaultRequestHeaders.Accept.ParseAdd("application/vnd.github+json");
                string json = await client.GetStringAsync(
                    "https://api.github.com/repos/deltahelpers/DRBackupCreator/releases/latest");
                var serializer = new JavaScriptSerializer();
                var release = serializer.Deserialize<System.Collections.Generic.Dictionary<string, object>>(json);
                var info = new ReleaseInfo
                {
                    Version = StripLeadingV(Convert.ToString(release["tag_name"])),
                    Notes = Convert.ToString(release.ContainsKey("body") ? release["body"] : ""),
                    Url = Convert.ToString(release.ContainsKey("html_url") ? release["html_url"] : "")
                };
                object assetsValue;
                var assets = release.TryGetValue("assets", out assetsValue)
                    ? assetsValue as System.Collections.ArrayList
                    : null;
                if (assets != null)
                {
                    foreach (object value in assets)
                    {
                        var asset = value as System.Collections.Generic.Dictionary<string, object>;
                        object assetName;
                        if (asset == null || !asset.TryGetValue("name", out assetName) ||
                            !String.Equals(Convert.ToString(assetName),
                            "drbcWindowsInstaller.exe", StringComparison.OrdinalIgnoreCase)) continue;
                        object assetUrl, digest;
                        if (asset.TryGetValue("browser_download_url", out assetUrl))
                            info.InstallerUrl = Convert.ToString(assetUrl);
                        info.Digest = asset.TryGetValue("digest", out digest) ? Convert.ToString(digest) : "";
                        break;
                    }
                }
                return info;
            }
        }

        public static async Task<string> DownloadInstallerAsync(ReleaseInfo release, string directory)
        {
            if (String.IsNullOrWhiteSpace(release.InstallerUrl))
                throw new InvalidOperationException("The latest release has no Windows installer.");
            if (String.IsNullOrWhiteSpace(release.Digest))
                throw new InvalidDataException("GitHub did not provide a SHA-256 digest for the installer.");
            Directory.CreateDirectory(directory);
            string path = Path.Combine(directory, "drbcWindowsInstaller.exe");
            using (var client = new HttpClient())
            using (var response = await client.GetAsync(release.InstallerUrl))
            {
                response.EnsureSuccessStatusCode();
                File.WriteAllBytes(path, await response.Content.ReadAsByteArrayAsync());
            }

            if (!String.IsNullOrWhiteSpace(release.Digest))
            {
                string expected = release.Digest;
                if (expected.StartsWith("sha256:", StringComparison.OrdinalIgnoreCase))
                    expected = expected.Substring(7);
                using (var sha = SHA256.Create())
                using (var stream = File.OpenRead(path))
                {
                    string actual = BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
                    if (!String.Equals(actual, expected, StringComparison.OrdinalIgnoreCase))
                    {
                        File.Delete(path);
                        throw new InvalidDataException("The installer SHA-256 digest did not match GitHub's release digest.");
                    }
                }
            }
            return path;
        }

        public static bool IsNewer(string candidate, string current)
        {
            System.Version a, b;
            return System.Version.TryParse(candidate, out a) &&
                   System.Version.TryParse(current, out b) && a > b;
        }

        private static bool RunTaskCommand(string arguments, out string output, out int exitCode)
        {
            using (var process = new Process())
            {
                process.StartInfo = new ProcessStartInfo
                {
                    FileName = "schtasks.exe",
                    Arguments = arguments,
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true
                };
                process.Start();
                string stdout = process.StandardOutput.ReadToEnd();
                string stderr = process.StandardError.ReadToEnd();
                if (!process.WaitForExit(20000))
                {
                    process.Kill();
                    output = "Task Scheduler command timed out.";
                    exitCode = -1;
                    return false;
                }
                output = String.IsNullOrWhiteSpace(stderr) ? stdout : stderr;
                exitCode = process.ExitCode;
                return true;
            }
        }

        private static void AppendLog(string file, string result)
        {
            File.AppendAllText(file, DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss ") + result + Environment.NewLine);
        }

        private static string StripComment(string line)
        {
            bool quoted = false;
            for (int i = 0; i < line.Length; i++)
            {
                if (line[i] == '"' && (i == 0 || line[i - 1] != '\\')) quoted = !quoted;
                if (line[i] == '#' && !quoted) return line.Substring(0, i);
            }
            return line;
        }

        private static string Unquote(string value)
        {
            if (value.Length >= 2 && value[0] == '"' && value[value.Length - 1] == '"')
                return value.Substring(1, value.Length - 2).Replace("\\\"", "\"").Replace("\\\\", "\\");
            if (value.Length >= 2 && value[0] == '\'' && value[value.Length - 1] == '\'')
                return value.Substring(1, value.Length - 2);
            return value;
        }

        private static string ResolveRelative(string baseDirectory, string value)
        {
            if (String.IsNullOrWhiteSpace(value)) return "";
            string expanded = Environment.ExpandEnvironmentVariables(value);
            return Path.IsPathRooted(expanded) ? expanded : Path.GetFullPath(Path.Combine(baseDirectory, expanded));
        }

        private static string FindFormatValue(string text, string key)
        {
            foreach (string line in text.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries))
            {
                string trimmed = line.Trim();
                if (trimmed.StartsWith(key + ":", StringComparison.OrdinalIgnoreCase))
                    return trimmed.Substring(key.Length + 1).Trim().Trim('"', '\'');
            }
            return null;
        }

        private static string Escape(string value)
        {
            return (value ?? "").Replace("\\", "\\\\").Replace("\"", "\\\"");
        }

        private static bool IsClock(string value)
        {
            DateTime parsed;
            return DateTime.TryParseExact(value, "HH:mm",
                System.Globalization.CultureInfo.InvariantCulture,
                System.Globalization.DateTimeStyles.None, out parsed);
        }

        private static string StripLeadingV(string text)
        {
            return !String.IsNullOrEmpty(text) && (text[0] == 'v' || text[0] == 'V')
                ? text.Substring(1)
                : text;
        }
    }

    internal sealed class ReleaseInfo
    {
        public string Version;
        public string Notes;
        public string Url;
        public string InstallerUrl;
        public string Digest;
    }
}
