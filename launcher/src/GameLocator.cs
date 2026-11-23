using System;
using System.IO;
using System.Collections.Generic;
using Microsoft.Win32;

namespace ClearSkyCoop.Launcher
{
    public static class GameLocator
    {
        private static readonly string[] CommonPaths = new string[]
        {
            @"D:\Games\S.T.A.L.K.E.R. Clear Sky",
            @"C:\Games\S.T.A.L.K.E.R. Clear Sky",
            @"E:\Games\S.T.A.L.K.E.R. Clear Sky",
            @"C:\Program Files (x86)\Steam\steamapps\common\STALKER Clear Sky",
            @"D:\Steam\steamapps\common\STALKER Clear Sky",
            @"D:\SteamLibrary\steamapps\common\STALKER Clear Sky",
            @"E:\SteamLibrary\steamapps\common\STALKER Clear Sky",
            @"C:\GOG Games\S.T.A.L.K.E.R. Clear Sky",
            @"D:\GOG Games\S.T.A.L.K.E.R. Clear Sky"
        };

        public static string DetectGamePath()
        {
            // 1. Check registry GSC Game World
            string regPath = ReadRegistry(@"SOFTWARE\GSC Game World\STALKER-CS", "InstallPath");
            if (IsValidGameDirectory(regPath)) return ModDeployer.NormalizeGamePath(regPath);

            regPath = ReadRegistry(@"SOFTWARE\WOW6432Node\GSC Game World\STALKER-CS", "InstallPath");
            if (IsValidGameDirectory(regPath)) return ModDeployer.NormalizeGamePath(regPath);

            // 2. Check GOG Registry
            regPath = ReadRegistry(@"SOFTWARE\GOG.com\Games\1435826982", "path");
            if (IsValidGameDirectory(regPath)) return ModDeployer.NormalizeGamePath(regPath);

            regPath = ReadRegistry(@"SOFTWARE\WOW6432Node\GOG.com\Games\1435826982", "path");
            if (IsValidGameDirectory(regPath)) return ModDeployer.NormalizeGamePath(regPath);

            // 3. Check Steam libraries
            string steamPath = ReadRegistry(@"SOFTWARE\Valve\Steam", "SteamPath", RegistryHive.CurrentUser);
            if (!string.IsNullOrEmpty(steamPath))
            {
                string candidate = Path.Combine(steamPath, @"steamapps\common\STALKER Clear Sky");
                if (IsValidGameDirectory(candidate)) return ModDeployer.NormalizeGamePath(candidate);

                // Read libraryfolders.vdf
                string vdf = Path.Combine(steamPath, @"steamapps\libraryfolders.vdf");
                if (File.Exists(vdf))
                {
                    try
                    {
                        foreach (string line in File.ReadAllLines(vdf))
                        {
                            if (line.Contains("\"path\""))
                            {
                                string[] parts = line.Split(new char[] { '"' }, StringSplitOptions.RemoveEmptyEntries);
                                if (parts.Length >= 4)
                                {
                                    string libPath = parts[3].Replace(@"\\", @"\");
                                    candidate = Path.Combine(libPath, @"steamapps\common\STALKER Clear Sky");
                                    if (IsValidGameDirectory(candidate)) return ModDeployer.NormalizeGamePath(candidate);
                                }
                            }
                        }
                    }
                    catch { }
                }
            }

            // 4. Check common hardcoded install paths
            foreach (string common in CommonPaths)
            {
                if (IsValidGameDirectory(common)) return ModDeployer.NormalizeGamePath(common);
            }

            // 5. Fallback: Check local test copy if running in project workspace
            try
            {
                string localHost = Path.GetFullPath(@"local\game-host");
                if (IsValidGameDirectory(localHost) && ModDeployer.IsModInstalled(localHost))
                {
                    return ModDeployer.NormalizeGamePath(localHost);
                }
            }
            catch { }

            return null;
        }

        public static bool IsValidGameDirectory(string path)
        {
            if (string.IsNullOrWhiteSpace(path)) return false;
            try
            {
                string norm = ModDeployer.NormalizeGamePath(path);
                if (!Directory.Exists(norm)) return false;
                string fsgame = Path.Combine(norm, "fsgame.ltx");
                if (!File.Exists(fsgame)) return false;

                // Check for either original game resources or patches or bin
                bool hasResources = Directory.Exists(Path.Combine(norm, "resources")) ||
                                    Directory.Exists(Path.Combine(norm, "patches")) ||
                                    Directory.Exists(Path.Combine(norm, "levels")) ||
                                    Directory.Exists(Path.Combine(norm, "bin"));

                return hasResources;
            }
            catch
            {
                return false;
            }
        }

        private static string ReadRegistry(string subKey, string valueName, RegistryHive hive = RegistryHive.LocalMachine)
        {
            try
            {
                using (RegistryKey baseKey = RegistryKey.OpenBaseKey(hive, RegistryView.Default))
                using (RegistryKey key = baseKey.OpenSubKey(subKey))
                {
                    if (key != null)
                    {
                        object val = key.GetValue(valueName);
                        if (val != null)
                        {
                            return val.ToString();
                        }
                    }
                }
            }
            catch { }
            return null;
        }
    }
}
