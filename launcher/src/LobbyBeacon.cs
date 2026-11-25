using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Collections.Concurrent;
using System.Collections.Generic;

namespace ClearSkyCoop.Launcher
{
    public class LobbyServerInfo
    {
        public string ServerName { get; set; }
        public string HostName { get; set; }
        public string IP { get; set; }
        public int Port { get; set; }
        public string Level { get; set; }
        public int CurrentPlayers { get; set; }
        public int MaxPlayers { get; set; }
        public DateTime LastSeenUtc { get; set; }

        public string DisplayText
        {
            get
            {
                return string.Format("{0} ({1}:{2}) - {3} [{4}/{5} players]",
                    ServerName, IP, Port, Level, CurrentPlayers, MaxPlayers);
            }
        }
    }

    public class LobbyBeaconService : IDisposable
    {
        public const int DiscoveryPort = 27015;
        private const string BeaconHeader = "CS_COOP_BEACON_V1";

        private UdpClient _listenerClient;
        private Thread _broadcastThread;
        private Thread _listenThread;
        private volatile bool _running;

        private readonly ConcurrentDictionary<string, LobbyServerInfo> _servers =
            new ConcurrentDictionary<string, LobbyServerInfo>();

        public event Action<LobbyServerInfo> OnServerUpdated;
        public event Action<string> OnServerRemoved;

        public LobbyServerInfo HostInfo { get; set; }
        public bool IsBroadcasting { get; set; }

        public void StartListener()
        {
            if (_running) return;
            _running = true;

            try
            {
                _listenerClient = new UdpClient();
                _listenerClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
                _listenerClient.Client.Bind(new IPEndPoint(IPAddress.Any, DiscoveryPort));

                _listenThread = new Thread(ListenLoop) { IsBackground = true, Name = "LobbyListener" };
                _listenThread.Start();
            }
            catch (Exception ex)
            {
                Console.WriteLine("Lobby listener bind error: " + ex.Message);
            }
        }

        public void StartBroadcaster(LobbyServerInfo hostInfo)
        {
            HostInfo = hostInfo;
            IsBroadcasting = true;

            if (_broadcastThread == null || !_broadcastThread.IsAlive)
            {
                _broadcastThread = new Thread(BroadcastLoop) { IsBackground = true, Name = "LobbyBroadcaster" };
                _broadcastThread.Start();
            }
        }

        public void StopBroadcaster()
        {
            IsBroadcasting = false;
        }

        private void BroadcastLoop()
        {
            using (UdpClient broadcaster = new UdpClient())
            {
                broadcaster.EnableBroadcast = true;
                IPEndPoint ep = new IPEndPoint(IPAddress.Broadcast, DiscoveryPort);

                while (_running && IsBroadcasting)
                {
                    if (HostInfo != null)
                    {
                        try
                        {
                            // Message format: HEADER|ServerName|HostName|GamePort|Level|CurPlayers|MaxPlayers
                            string msg = string.Format("{0}|{1}|{2}|{3}|{4}|{5}|{6}",
                                BeaconHeader,
                                Sanitize(HostInfo.ServerName),
                                Sanitize(HostInfo.HostName),
                                HostInfo.Port,
                                Sanitize(HostInfo.Level),
                                HostInfo.CurrentPlayers,
                                HostInfo.MaxPlayers);

                            byte[] data = Encoding.UTF8.GetBytes(msg);
                            broadcaster.Send(data, data.Length, ep);
                        }
                        catch { }
                    }
                    Thread.Sleep(1500);
                }
            }
        }

        private void ListenLoop()
        {
            IPEndPoint remoteEp = new IPEndPoint(IPAddress.Any, 0);

            while (_running)
            {
                try
                {
                    if (_listenerClient == null || _listenerClient.Client == null) break;

                    // Non-blocking check or receive
                    if (_listenerClient.Available > 0)
                    {
                        byte[] data = _listenerClient.Receive(ref remoteEp);
                        string text = Encoding.UTF8.GetString(data);

                        if (text.StartsWith(BeaconHeader))
                        {
                            string[] parts = text.Split('|');
                            if (parts.Length >= 7)
                            {
                                int gamePort = int.Parse(parts[3]);
                                string key = remoteEp.Address.ToString() + ":" + gamePort;

                                LobbyServerInfo info = new LobbyServerInfo
                                {
                                    ServerName = parts[1],
                                    HostName = parts[2],
                                    IP = remoteEp.Address.ToString(),
                                    Port = gamePort,
                                    Level = parts[4],
                                    CurrentPlayers = int.Parse(parts[5]),
                                    MaxPlayers = int.Parse(parts[6]),
                                    LastSeenUtc = DateTime.UtcNow
                                };

                                _servers[key] = info;
                                if (OnServerUpdated != null)
                                {
                                    OnServerUpdated(info);
                                }
                            }
                        }
                    }

                    // Check for aged-out servers (older than 4 seconds)
                    DateTime cutoff = DateTime.UtcNow.AddSeconds(-4);
                    foreach (var kvp in _servers)
                    {
                        if (kvp.Value.LastSeenUtc < cutoff)
                        {
                            LobbyServerInfo removed;
                            if (_servers.TryRemove(kvp.Key, out removed))
                            {
                                if (OnServerRemoved != null)
                                {
                                    OnServerRemoved(kvp.Key);
                                }
                            }
                        }
                    }

                    Thread.Sleep(100);
                }
                catch (ThreadAbortException) { break; }
                catch (Exception)
                {
                    Thread.Sleep(500);
                }
            }
        }

        public List<LobbyServerInfo> GetActiveServers()
        {
            return new List<LobbyServerInfo>(_servers.Values);
        }

        private static string Sanitize(string val)
        {
            if (string.IsNullOrEmpty(val)) return "Unknown";
            return val.Replace('|', '_');
        }

        public void Dispose()
        {
            _running = false;
            IsBroadcasting = false;

            try
            {
                if (_listenerClient != null)
                {
                    _listenerClient.Close();
                    _listenerClient = null;
                }
            }
            catch { }
        }
    }
}
