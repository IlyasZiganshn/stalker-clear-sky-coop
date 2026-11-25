using System;
using System.IO;
using System.Diagnostics;

namespace ClearSkyCoop.Launcher
{
    public static class SessionLauncher
    {
        private static void EnableInGameLobbyFlag(string gamePath)
        {
            try
            {
                string appData = Path.Combine(gamePath, "_appdata_");
                if (!Directory.Exists(appData)) Directory.CreateDirectory(appData);
                File.WriteAllText(Path.Combine(appData, "coop_lobby_mode"), "1");
            }
            catch { }
        }

        public static string BuildHostArgs(int port, bool skipIntro = false)
        {
            string intro = skipIntro ? " -nogameintro" : "";
            return string.Format("-cs -nointro{0} -force_flushlog -exec \"coop_listen {1}; start server(all/single/alife/new)\"", intro, port);
        }

        public static string BuildJoinArgs(string hostIp, int port, string playerName, bool skipIntro = false)
        {
            string intro = skipIntro ? " -nogameintro" : "";
            string cleanName = string.IsNullOrEmpty(playerName) ? "Player" : playerName.Replace(" ", "_");
            return string.Format("-cs -nointro{0} -force_flushlog -exec \"coop_connect {1}:{2} {3}; start server(all/single/alife/new)\"",
                intro, hostIp, port, cleanName);
        }

        public static Process StartHost(string rawGamePath, int port, out string error)
        {
            return StartHost(rawGamePath, port, false, out error);
        }

        public static Process StartHost(string rawGamePath, int port, bool skipIntro, out string error)
        {
            error = null;
            try
            {
                string gamePath = ModDeployer.NormalizeGamePath(rawGamePath);
                if (string.IsNullOrEmpty(gamePath) || !Directory.Exists(gamePath))
                {
                    error = "Invalid game directory: " + rawGamePath;
                    return null;
                }

                string exe = Path.Combine(gamePath, @"bin\x64\xrEngine.exe");
                if (!File.Exists(exe))
                {
                    error = "xrEngine.exe not found at:\n" + exe + "\n\nPlease deploy mod files under the Settings tab.";
                    return null;
                }

                ProcessStartInfo psi = new ProcessStartInfo
                {
                    FileName = exe,
                    Arguments = BuildHostArgs(port, skipIntro),
                    WorkingDirectory = gamePath,
                    UseShellExecute = false,
                    CreateNoWindow = false
                };

                EnableInGameLobbyFlag(gamePath);
                return Process.Start(psi);
            }
            catch (Exception ex)
            {
                error = ex.Message;
                return null;
            }
        }

        public static Process StartClient(string rawGamePath, string hostIp, int port, string playerName, out string error)
        {
            return StartClient(rawGamePath, hostIp, port, playerName, false, out error);
        }

        public static Process StartClient(string rawGamePath, string hostIp, int port, string playerName, bool skipIntro, out string error)
        {
            error = null;
            try
            {
                string gamePath = ModDeployer.NormalizeGamePath(rawGamePath);
                if (string.IsNullOrEmpty(gamePath) || !Directory.Exists(gamePath))
                {
                    error = "Invalid game directory: " + rawGamePath;
                    return null;
                }

                string exe = Path.Combine(gamePath, @"bin\x64\xrEngine.exe");
                if (!File.Exists(exe))
                {
                    error = "xrEngine.exe not found at:\n" + exe + "\n\nPlease deploy mod files under the Settings tab.";
                    return null;
                }

                ProcessStartInfo psi = new ProcessStartInfo
                {
                    FileName = exe,
                    Arguments = BuildJoinArgs(hostIp, port, playerName, skipIntro),
                    WorkingDirectory = gamePath,
                    UseShellExecute = false,
                    CreateNoWindow = false
                };

                EnableInGameLobbyFlag(gamePath);
                return Process.Start(psi);
            }
            catch (Exception ex)
            {
                error = ex.Message;
                return null;
            }
        }
    }
}
