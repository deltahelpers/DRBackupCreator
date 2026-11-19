using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace DRBackupCreator
{
    internal sealed class SlotFile
    {
        public int Chapter { get; private set; }
        public int Index { get; private set; }
        public string Path { get; private set; }
        public long Size { get; private set; }
        public bool IsCompletion { get { return Index >= 5; } }
        public int PairedIndex { get { return 9 - Index; } }

        public SlotFile(int chapter, int index, string path, long size)
        {
            Chapter = chapter;
            Index = index;
            Path = path;
            Size = size;
        }
    }

    internal sealed class BackupInfo
    {
        public string Path { get; set; }
        public string Timestamp { get; set; }
        public int SlotCount { get; set; }
        public long TotalSize { get; set; }
    }

    internal static class BackupCore
    {
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct ShellFileOperation
        {
            public IntPtr Window;
            public uint Operation;
            [MarshalAs(UnmanagedType.LPWStr)] public string From;
            [MarshalAs(UnmanagedType.LPWStr)] public string To;
            public ushort Flags;
            [MarshalAs(UnmanagedType.Bool)] public bool Aborted;
            public IntPtr NameMappings;
            [MarshalAs(UnmanagedType.LPWStr)] public string ProgressTitle;
        }

        [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
        private static extern int SHFileOperation(ref ShellFileOperation operation);

        private static readonly string SaveDirectory =
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "DELTARUNE");

        public static string GetSaveDirectory()
        {
            return Directory.Exists(SaveDirectory) ? SaveDirectory : null;
        }

        public static bool IsGameRunning()
        {
            return Process.GetProcessesByName("DELTARUNE").Length != 0;
        }

        public static List<SlotFile> ScanSlots(string directory)
        {
            var slots = new List<SlotFile>();
            if (String.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory)) return slots;

            foreach (string file in Directory.EnumerateFiles(directory))
            {
                string name = System.IO.Path.GetFileNameWithoutExtension(file);
                if (!name.StartsWith("filech", StringComparison.Ordinal)) continue;
                string[] parts = name.Substring(6).Split('_');
                int chapter, index;
                if (parts.Length != 2 ||
                    !Int32.TryParse(parts[0], out chapter) ||
                    !Int32.TryParse(parts[1], out index)) continue;

                slots.Add(new SlotFile(chapter, index, file, new FileInfo(file).Length));
            }

            slots.Sort((left, right) =>
            {
                int chapter = left.Chapter.CompareTo(right.Chapter);
                return chapter != 0 ? chapter : left.Index.CompareTo(right.Index);
            });
            return slots;
        }

        public static void GroupBySave(List<SlotFile> slots)
        {
            slots.Sort((left, right) =>
            {
                int leftGroup = left.Chapter * 10 + Math.Min(left.Index, left.PairedIndex);
                int rightGroup = right.Chapter * 10 + Math.Min(right.Index, right.PairedIndex);
                int group = leftGroup.CompareTo(rightGroup);
                if (group != 0) return group;
                return left.IsCompletion.CompareTo(right.IsCompletion);
            });
        }

        public static string CreateBackup(string backupDirectory, IEnumerable<SlotFile> slots)
        {
            List<SlotFile> files = slots.ToList();
            if (files.Count == 0) return null;
            Directory.CreateDirectory(backupDirectory);

            string stem = "backup_" + DateTime.Now.ToString("yyyy-MM-dd_HH-mm-ss");
            string name = stem;
            for (int suffix = 2; Directory.Exists(System.IO.Path.Combine(backupDirectory, name)) ||
                File.Exists(System.IO.Path.Combine(backupDirectory, name)); suffix++)
                name = stem + "_" + suffix;

            string fullPath = System.IO.Path.Combine(backupDirectory, name);
            Directory.CreateDirectory(fullPath);
            try
            {
                foreach (SlotFile slot in files)
                    File.Copy(slot.Path, System.IO.Path.Combine(fullPath, System.IO.Path.GetFileName(slot.Path)), false);
            }
            catch (Exception copyError)
            {
                try
                {
                    Directory.Delete(fullPath, true);
                }
                catch (Exception cleanupError)
                {
                    throw new AggregateException("Backup creation failed and the partial folder could not be removed.",
                        copyError, cleanupError);
                }
                throw;
            }
            return name;
        }

        public static List<BackupInfo> ListBackups(string parentDirectory)
        {
            var backups = new List<BackupInfo>();
            if (!Directory.Exists(parentDirectory)) return backups;

            foreach (string directory in Directory.EnumerateDirectories(parentDirectory))
            {
                string name = System.IO.Path.GetFileName(directory);
                if (!name.StartsWith("backup_", StringComparison.Ordinal)) continue;
                FileInfo[] files = new DirectoryInfo(directory).GetFiles();
                backups.Add(new BackupInfo
                {
                    Path = directory,
                    Timestamp = name.Substring(7),
                    SlotCount = files.Length,
                    TotalSize = files.Sum(file => file.Length)
                });
            }
            backups.Sort((left, right) => StringComparer.Ordinal.Compare(right.Timestamp, left.Timestamp));
            return backups;
        }

        public static List<BackupInfo> ListAllBackups(string primaryDirectory)
        {
            var result = ListBackups(primaryDirectory);
            string legacy = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            if (!String.IsNullOrWhiteSpace(legacy) &&
                !String.Equals(legacy, primaryDirectory, StringComparison.OrdinalIgnoreCase))
            {
                var names = new HashSet<string>(result.Select(item => System.IO.Path.GetFileName(item.Path)),
                    StringComparer.OrdinalIgnoreCase);
                result.AddRange(ListBackups(legacy).Where(item =>
                    names.Add(System.IO.Path.GetFileName(item.Path))));
            }
            result.Sort((left, right) => StringComparer.Ordinal.Compare(right.Timestamp, left.Timestamp));
            return result;
        }

        public static int RestoreBackup(string backupPath, string targetDirectory)
        {
            if (!Directory.Exists(backupPath)) throw new DirectoryNotFoundException(backupPath);
            if (!Directory.Exists(targetDirectory)) throw new DirectoryNotFoundException(targetDirectory);

            int restored = 0;
            foreach (string source in Directory.EnumerateFiles(backupPath))
            {
                File.Copy(source, System.IO.Path.Combine(targetDirectory, System.IO.Path.GetFileName(source)), true);
                restored++;
            }
            return restored;
        }

        public static void DeleteBackup(string backupPath)
        {
            var operation = new ShellFileOperation
            {
                Operation = 3,
                From = backupPath + "\0\0",
                Flags = 0x0054
            };
            int result = SHFileOperation(ref operation);
            if (result != 0 || operation.Aborted)
                throw new IOException("Windows could not move the backup to the Recycle Bin (error " + result + ").");
        }

        public static string FormatFileSize(long bytes)
        {
            string[] units = { "B", "KB", "MB", "GB" };
            double size = bytes;
            int unit = 0;
            while (size >= 1024 && unit < units.Length - 1)
            {
                size /= 1024;
                unit++;
            }
            return size.ToString("0.0") + " " + units[unit];
        }
    }
}
