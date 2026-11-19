using System;
using System.IO;
using System.Linq;
using DRBackupCreator;

internal static class Program
{
    private static int Main()
    {
        string root = Path.Combine(Path.GetTempPath(), "drbc-dotnet-tests-" + Guid.NewGuid().ToString("N"));
        string saves = Path.Combine(root, "DELTARUNE");
        string backups = Path.Combine(root, "backups");
        string target = Path.Combine(root, "restore");
        Directory.CreateDirectory(saves);
        Directory.CreateDirectory(target);

        try
        {
            File.WriteAllText(Path.Combine(saves, "filech1_0.dat"), "save");
            File.WriteAllText(Path.Combine(saves, "filech1_5.dat"), "completion");
            File.WriteAllText(Path.Combine(saves, "filech2_1.dat"), "other");
            File.WriteAllText(Path.Combine(saves, "notes.txt"), "not a slot");

            var slots = BackupCore.ScanSlots(saves);
            Check(slots.Count == 3, "slot scan filters non-save files");
            BackupCore.GroupBySave(slots);
            Check(slots[0].Index == 0 && slots[1].Index == 5, "save slots group in save/completion order");

            string name = BackupCore.CreateBackup(backups, slots);
            Check(name.StartsWith("backup_", StringComparison.Ordinal), "backup receives timestamped name");
            var listed = BackupCore.ListBackups(backups);
            Check(listed.Count == 1 && listed[0].SlotCount == 3, "backup listing counts files");
            Check(BackupCore.RestoreBackup(listed[0].Path, target) == 3, "restore copies every slot");
            Check(File.ReadAllText(Path.Combine(target, "filech1_0.dat")) == "save", "restore preserves content");
            Check(BackupCore.CreateBackup(backups, Enumerable.Empty<SlotFile>()) == null,
                "empty backup input is rejected");

            Console.WriteLine("All .NET backup core tests passed.");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
        finally
        {
            if (Directory.Exists(root)) Directory.Delete(root, true);
        }
    }

    private static void Check(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException("Failed: " + message);
    }
}
