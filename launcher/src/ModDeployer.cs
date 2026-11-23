using System;
using System.IO;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text.RegularExpressions;

namespace ClearSkyCoop.Launcher
{
    public static class ModDeployer
    {
        public static string NormalizeGamePath(string rawPath)
        {
            if (string.IsNullOrWhiteSpace(rawPath)) return string.Empty;

            string path = rawPath.Trim();
            // Strip quotes
            if ((path.StartsWith("\"") && path.EndsWith("\"")) ||
                (path.StartsWith("'") && path.EndsWith("'")))
            {
                path = path.Substring(1, path.Length - 2).Trim();
            }

            path = path.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);

            try
            {
                if (Directory.Exists(path))
                {
                    // Check if fsgame.ltx is in this directory
                    if (File.Exists(Path.Combine(path, "fsgame.ltx")))
                    {
                        return Path.GetFullPath(path);
                    }

                    // Check if user selected bin, bin\x64, bin\x86, etc.
                    if (path.EndsWith(@"\bin", StringComparison.OrdinalIgnoreCase) ||
                        path.EndsWith(@"\bin\x64", StringComparison.OrdinalIgnoreCase) ||
                        path.EndsWith(@"\bin\x86", StringComparison.OrdinalIgnoreCase))
                    {
                        DirectoryInfo parent = Directory.GetParent(path);
                        if (parent != null && File.Exists(Path.Combine(parent.FullName, "fsgame.ltx")))
                        {
                            return parent.FullName;
                        }
                        if (parent != null)
                        {
                            DirectoryInfo grandParent = parent.Parent;
                            if (grandParent != null && File.Exists(Path.Combine(grandParent.FullName, "fsgame.ltx")))
                            {
                                return grandParent.FullName;
                            }
                        }
                    }

                    // If user selected any subfolder where parent has fsgame.ltx
                    DirectoryInfo p = Directory.GetParent(path);
                    if (p != null && File.Exists(Path.Combine(p.FullName, "fsgame.ltx")))
                    {
                        return p.FullName;
                    }

                    return Path.GetFullPath(path);
                }
            }
            catch { }

            return path;
        }

        public static bool IsModInstalled(string gamePath)
        {
            string normPath = NormalizeGamePath(gamePath);
            if (string.IsNullOrEmpty(normPath) || !Directory.Exists(normPath)) return false;

            string engineExe = Path.Combine(normPath, @"bin\x64\xrEngine.exe");
            string netServerDll = Path.Combine(normPath, @"bin\x64\xrNetServer.dll");
            string gameDll = Path.Combine(normPath, @"bin\x64\xrGame.dll");
            string fsgame = Path.Combine(normPath, "fsgame.ltx");

            return File.Exists(engineExe) && File.Exists(netServerDll) && File.Exists(gameDll) && File.Exists(fsgame);
        }

        public static string GetModSourceDir()
        {
            string checkedLog;
            return GetModSourceDir(out checkedLog);
        }

        public static string GetModSourceDir(out string checkedLog)
        {
            List<string> candidateBases = new List<string>();

            try
            {
                string loc = Path.GetDirectoryName(typeof(ModDeployer).Assembly.Location);
                if (!string.IsNullOrEmpty(loc) && !candidateBases.Contains(loc))
                    candidateBases.Add(loc);
            }
            catch { }

            string baseDir = AppDomain.CurrentDomain.BaseDirectory;
            if (!string.IsNullOrEmpty(baseDir) && !candidateBases.Contains(baseDir))
                candidateBases.Add(baseDir);

            try
            {
                string curDir = Directory.GetCurrentDirectory();
                if (!string.IsNullOrEmpty(curDir) && !candidateBases.Contains(curDir))
                    candidateBases.Add(curDir);
            }
            catch { }

            string[] subPaths = new string[]
            {
                "",
                @"x64",
                @"bin\x64",
                @"bin",
                @"engine\src\bin\x64\Release",
                @"local\game-host\bin\x64",
                @"local\game-client\bin\x64"
            };

            List<string> testedPaths = new List<string>();

            foreach (string b in candidateBases)
            {
                DirectoryInfo dir = new DirectoryInfo(b);
                for (int level = 0; level < 6 && dir != null; level++)
                {
                    foreach (string sub in subPaths)
                    {
                        try
                        {
                            string candidate = string.IsNullOrEmpty(sub)
                                ? dir.FullName
                                : Path.GetFullPath(Path.Combine(dir.FullName, sub));

                            if (!testedPaths.Contains(candidate))
                            {
                                testedPaths.Add(candidate);
                                if (Directory.Exists(candidate))
                                {
                                    string engineExe = Path.Combine(candidate, "xrEngine.exe");
                                    string netServer = Path.Combine(candidate, "xrNetServer.dll");
                                    if (File.Exists(engineExe) && File.Exists(netServer))
                                    {
                                        checkedLog = string.Join("\n", testedPaths.ToArray());
                                        return candidate;
                                    }
                                }
                            }
                        }
                        catch { }
                    }
                    dir = dir.Parent;
                }
            }

            // Also check known workspace paths if running under developer machine
            string[] knownDevPaths = new string[]
            {
                @"D:\projects\personal\Clear Sky - Coop\bin\x64",
                @"D:\projects\personal\Clear Sky - Coop\engine\src\bin\x64\Release",
                @"D:\projects\personal\Clear Sky - Coop\local\game-host\bin\x64"
            };

            foreach (string devPath in knownDevPaths)
            {
                if (!testedPaths.Contains(devPath))
                {
                    testedPaths.Add(devPath);
                    if (Directory.Exists(devPath))
                    {
                        string engineExe = Path.Combine(devPath, "xrEngine.exe");
                        string netServer = Path.Combine(devPath, "xrNetServer.dll");
                        if (File.Exists(engineExe) && File.Exists(netServer))
                        {
                            checkedLog = string.Join("\n", testedPaths.ToArray());
                            return devPath;
                        }
                    }
                }
            }

            checkedLog = string.Join("\n", testedPaths.ToArray());
            return null;
        }

        public static string GetGamedataSourceDir()
        {
            List<string> candidateBases = new List<string>();

            try
            {
                string loc = Path.GetDirectoryName(typeof(ModDeployer).Assembly.Location);
                if (!string.IsNullOrEmpty(loc) && !candidateBases.Contains(loc))
                    candidateBases.Add(loc);
            }
            catch { }

            string baseDir = AppDomain.CurrentDomain.BaseDirectory;
            if (!string.IsNullOrEmpty(baseDir) && !candidateBases.Contains(baseDir))
                candidateBases.Add(baseDir);

            foreach (string b in candidateBases)
            {
                DirectoryInfo dir = new DirectoryInfo(b);
                for (int i = 0; i < 6 && dir != null; i++)
                {
                    string candidate = Path.Combine(dir.FullName, "gamedata");
                    if (Directory.Exists(candidate))
                    {
                        return candidate;
                    }
                    dir = dir.Parent;
                }
            }

            string devGamedata = @"D:\projects\personal\Clear Sky - Coop\gamedata";
            if (Directory.Exists(devGamedata)) return devGamedata;

            return null;
        }

        public static void SafeCopyFile(string source, string destination)
        {
            if (string.Equals(Path.GetFullPath(source), Path.GetFullPath(destination), StringComparison.OrdinalIgnoreCase))
                return;

            string destDir = Path.GetDirectoryName(destination);
            if (!string.IsNullOrEmpty(destDir) && !Directory.Exists(destDir))
            {
                Directory.CreateDirectory(destDir);
            }

            if (File.Exists(destination))
            {
                try
                {
                    FileAttributes attrs = File.GetAttributes(destination);
                    if ((attrs & FileAttributes.ReadOnly) != 0)
                    {
                        File.SetAttributes(destination, attrs & ~FileAttributes.ReadOnly);
                    }
                }
                catch { }
            }

            File.Copy(source, destination, true);

            try
            {
                FileAttributes attrs = File.GetAttributes(destination);
                if ((attrs & FileAttributes.ReadOnly) != 0)
                {
                    File.SetAttributes(destination, attrs & ~FileAttributes.ReadOnly);
                }
            }
            catch { }
        }

        public static void SafeCopyDirectory(string sourceDir, string destinationDir)
        {
            DirectoryInfo dir = new DirectoryInfo(sourceDir);
            if (!dir.Exists) return;

            if (!Directory.Exists(destinationDir))
            {
                Directory.CreateDirectory(destinationDir);
            }

            try
            {
                FileAttributes dirAttrs = File.GetAttributes(destinationDir);
                if ((dirAttrs & FileAttributes.ReadOnly) != 0)
                {
                    File.SetAttributes(destinationDir, dirAttrs & ~FileAttributes.ReadOnly);
                }
            }
            catch { }

            foreach (FileInfo file in dir.GetFiles())
            {
                string targetFilePath = Path.Combine(destinationDir, file.Name);
                SafeCopyFile(file.FullName, targetFilePath);
            }

            foreach (DirectoryInfo subDir in dir.GetDirectories())
            {
                string newDestinationDir = Path.Combine(destinationDir, subDir.Name);
                SafeCopyDirectory(subDir.FullName, newDestinationDir);
            }
        }

        public static bool DeployMod(string gamePath, out string error)
        {
            error = null;
            try
            {
                string normPath = NormalizeGamePath(gamePath);
                if (string.IsNullOrEmpty(normPath) || !Directory.Exists(normPath))
                {
                    error = "Game path does not exist:\n" + gamePath;
                    return false;
                }

                string fsgame = Path.Combine(normPath, "fsgame.ltx");
                if (!File.Exists(fsgame))
                {
                    error = "Selected directory does not appear to be S.T.A.L.K.E.R.: Clear Sky.\n'fsgame.ltx' was not found in:\n" + normPath;
                    return false;
                }

                // Check for running game instances that lock files
                try
                {
                    Process[] running = Process.GetProcessesByName("xrEngine");
                    if (running != null && running.Length > 0)
                    {
                        error = "S.T.A.L.K.E.R.: Clear Sky (xrEngine.exe) is currently running.\nPlease close the game before installing or updating mod files.";
                        return false;
                    }
                }
                catch { }

                string checkedLog;
                string sourceBin = GetModSourceDir(out checkedLog);
                if (string.IsNullOrEmpty(sourceBin))
                {
                    error = "Mod binaries (bin\\x64\\xrEngine.exe) not found in launcher distribution.\n\nChecked paths:\n" + checkedLog;
                    return false;
                }

                // 1. Copy x64 binaries into <gamePath>\bin\x64
                string targetBin = Path.Combine(normPath, @"bin\x64");
                SafeCopyDirectory(sourceBin, targetBin);

                // 2. Deploy gamedata scripts if available
                string srcGamedata = GetGamedataSourceDir();
                if (!string.IsNullOrEmpty(srcGamedata) && Directory.Exists(srcGamedata))
                {
                    SafeCopyDirectory(srcGamedata, Path.Combine(normPath, "gamedata"));
                }

                // 3. Configure fsgame.ltx to use local _appdata_ and allow loose gamedata
                try
                {
                    FileAttributes fsAttrs = File.GetAttributes(fsgame);
                    if ((fsAttrs & FileAttributes.ReadOnly) != 0)
                    {
                        File.SetAttributes(fsgame, fsAttrs & ~FileAttributes.ReadOnly);
                    }

                    // Create backup if not already saved
                    string fsgameBak = Path.Combine(normPath, "fsgame.ltx.coop_backup");
                    if (!File.Exists(fsgameBak))
                    {
                        File.Copy(fsgame, fsgameBak, false);
                    }

                    string[] lines = File.ReadAllLines(fsgame);
                    bool appDataFound = false;
                    bool gameDataFound = false;

                    Regex appDataRegex = new Regex(@"^\s*\$app_data_root\$\s*=.*", RegexOptions.IgnoreCase);
                    Regex gameDataRegex = new Regex(@"^\s*\$game_data\$\s*=.*", RegexOptions.IgnoreCase);

                    List<string> updatedLines = new List<string>();
                    foreach (string line in lines)
                    {
                        if (appDataRegex.IsMatch(line))
                        {
                            updatedLines.Add("$app_data_root$         = true|  false| $fs_root$|            _appdata_\\");
                            appDataFound = true;
                        }
                        else if (gameDataRegex.IsMatch(line))
                        {
                            updatedLines.Add("$game_data$             = true|  true|  $fs_root$|            gamedata\\");
                            gameDataFound = true;
                        }
                        else
                        {
                            updatedLines.Add(line);
                        }
                    }

                    if (!appDataFound)
                    {
                        updatedLines.Insert(0, "$app_data_root$         = true|  false| $fs_root$|            _appdata_\\");
                    }
                    if (!gameDataFound)
                    {
                        updatedLines.Add("$game_data$             = true|  true|  $fs_root$|            gamedata\\");
                    }

                    File.WriteAllLines(fsgame, updatedLines.ToArray());
                }
                catch (Exception ex)
                {
                    error = "Failed to update fsgame.ltx: " + ex.Message;
                    return false;
                }

                // 4. Ensure _appdata_ directories exist
                string appDataDir = Path.Combine(normPath, "_appdata_");
                string logsDir = Path.Combine(appDataDir, "logs");
                string savesDir = Path.Combine(appDataDir, "savedgames");
                string shotsDir = Path.Combine(appDataDir, "screenshots");

                if (!Directory.Exists(appDataDir)) Directory.CreateDirectory(appDataDir);
                if (!Directory.Exists(logsDir)) Directory.CreateDirectory(logsDir);
                if (!Directory.Exists(savesDir)) Directory.CreateDirectory(savesDir);
                if (!Directory.Exists(shotsDir)) Directory.CreateDirectory(shotsDir);

                // 5. Ensure default user.ltx has rs_always_active 1 (prevents stalling on alt-tab)
                try
                {
                    string userLtx = Path.Combine(appDataDir, "user.ltx");
                    if (!File.Exists(userLtx))
                    {
                        File.WriteAllText(userLtx, "rs_always_active 1\r\n");
                    }
                    else
                    {
                        string userContent = File.ReadAllText(userLtx);
                        if (!userContent.Contains("rs_always_active"))
                        {
                            File.AppendAllText(userLtx, "\r\nrs_always_active 1\r\n");
                        }
                    }
                }
                catch { }

                return true;
            }
            catch (Exception ex)
            {
                error = ex.Message;
                return false;
            }
        }
    }
}
