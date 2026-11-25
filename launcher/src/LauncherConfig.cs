using System;
using System.IO;
using System.Collections.Generic;
using System.Reflection;

namespace ClearSkyCoop.Launcher
{
    public class LauncherConfig
    {
        public static string GetConfigPath()
        {
            try
            {
                string asmLoc = Assembly.GetExecutingAssembly().Location;
                if (!string.IsNullOrEmpty(asmLoc))
                {
                    string dir = Path.GetDirectoryName(asmLoc);
                    if (!string.IsNullOrEmpty(dir) && Directory.Exists(dir))
                    {
                        return Path.Combine(dir, "launcher_config.ini");
                    }
                }
            }
            catch { }
            return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "launcher_config.ini");
        }

        private static string ConfigPath
        {
            get { return GetConfigPath(); }
        }

        public string PlayerName { get; set; }
        public string GamePath { get; set; }
        public string DirectIp { get; set; }
        public string DirectPort { get; set; }
        public string HostServerName { get; set; }
        public string HostPlayerName { get; set; }
        public int HostPort { get; set; }
        public bool HostBroadcast { get; set; }
        public bool HostSkipIntro { get; set; }
        public bool GlobalSkipIntro { get; set; }
        public List<string> RecentServers { get; set; }

        public LauncherConfig()
        {
            PlayerName = Environment.UserName;
            GamePath = "";
            DirectIp = "127.0.0.1";
            DirectPort = "27016";
            HostServerName = Environment.UserName + "'s Clear Sky Campaign";
            HostPlayerName = Environment.UserName;
            HostPort = 27016;
            HostBroadcast = true;
            HostSkipIntro = false;
            GlobalSkipIntro = false;
            RecentServers = new List<string>();
        }

        public static LauncherConfig Load()
        {
            LauncherConfig config = new LauncherConfig();
            if (!File.Exists(ConfigPath)) return config;

            try
            {
                string[] lines = File.ReadAllLines(ConfigPath);
                foreach (string line in lines)
                {
                    string trimmed = line.Trim();
                    if (string.IsNullOrEmpty(trimmed) || trimmed.StartsWith("#") || trimmed.StartsWith(";")) continue;
                    int idx = trimmed.IndexOf('=');
                    if (idx <= 0) continue;
                    string key = trimmed.Substring(0, idx).Trim();
                    string val = trimmed.Substring(idx + 1).Trim();

                    switch (key.ToLowerInvariant())
                    {
                        case "playername": config.PlayerName = val; break;
                        case "gamepath": config.GamePath = val; break;
                        case "directip": config.DirectIp = val; break;
                        case "directport": config.DirectPort = val; break;
                        case "hostservername": config.HostServerName = val; break;
                        case "hostplayername": config.HostPlayerName = val; break;
                        case "hostport": int hp; if (int.TryParse(val, out hp)) config.HostPort = hp; break;
                        case "hostbroadcast": config.HostBroadcast = (val.ToLowerInvariant() == "true" || val == "1"); break;
                        case "hostskipintro": config.HostSkipIntro = (val.ToLowerInvariant() == "true" || val == "1"); break;
                        case "globalskipintro": config.GlobalSkipIntro = (val.ToLowerInvariant() == "true" || val == "1"); break;
                        case "recentservers":
                            if (!string.IsNullOrEmpty(val))
                            {
                                string[] parts = val.Split(new char[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
                                foreach (var p in parts)
                                {
                                    string s = p.Trim();
                                    if (!string.IsNullOrEmpty(s) && !config.RecentServers.Contains(s))
                                        config.RecentServers.Add(s);
                                }
                            }
                            break;
                    }
                }
            }
            catch { }
            return config;
        }

        public void Save()
        {
            try
            {
                List<string> lines = new List<string>
                {
                    "# S.T.A.L.K.E.R. Clear Sky Co-op Launcher Settings",
                    "PlayerName=" + PlayerName,
                    "GamePath=" + GamePath,
                    "DirectIp=" + DirectIp,
                    "DirectPort=" + DirectPort,
                    "HostServerName=" + HostServerName,
                    "HostPlayerName=" + HostPlayerName,
                    "HostPort=" + HostPort,
                    "HostBroadcast=" + (HostBroadcast ? "1" : "0"),
                    "HostSkipIntro=" + (HostSkipIntro ? "1" : "0"),
                    "GlobalSkipIntro=" + (GlobalSkipIntro ? "1" : "0"),
                    "RecentServers=" + string.Join(",", RecentServers.ToArray())
                };
                File.WriteAllLines(ConfigPath, lines);
            }
            catch { }
        }

        public void AddRecentServer(string ip, int port)
        {
            string entry = ip + ":" + port;
            RecentServers.Remove(entry);
            RecentServers.Insert(0, entry);
            while (RecentServers.Count > 5) RecentServers.RemoveAt(RecentServers.Count - 1);
            Save();
        }
    }
}
