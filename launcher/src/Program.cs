using System;
using System.IO;
using System.Threading;
using System.Diagnostics;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace ClearSkyCoop.Launcher
{
    public static class Program
    {
        [DllImport("user32.dll")]
        private static extern bool SetProcessDPIAware();

        [STAThread]
        public static int Main(string[] args)
        {
            try
            {
                SetProcessDPIAware();
            }
            catch { }
            if (args != null && args.Length > 0)
            {
                string cmd = args[0].ToLowerInvariant();

                if (cmd == "--detect" || cmd == "-detect")
                {
                    string path = GameLocator.DetectGamePath();
                    if (!string.IsNullOrEmpty(path))
                    {
                        Console.WriteLine(path);
                        return 0;
                    }
                    Console.Error.WriteLine("Clear Sky installation not detected");
                    return 1;
                }

                if (cmd == "--start-host")
                {
                    int port = args.Length >= 2 ? int.Parse(args[1]) : 27016;
                    string gamePath = GameLocator.DetectGamePath();
                    string err;
                    Process p = SessionLauncher.StartHost(gamePath, port, true, out err);
                    if (p != null)
                    {
                        Console.WriteLine("HOST_STARTED:" + p.Id);
                        return 0;
                    }
                    Console.Error.WriteLine("ERROR:" + err);
                    return 1;
                }

                if (cmd == "--host-cmd" && args.Length >= 2)
                {
                    int port = int.Parse(args[1]);
                    Console.WriteLine(SessionLauncher.BuildHostArgs(port));
                    return 0;
                }

                if (cmd == "--join-cmd" && args.Length >= 4)
                {
                    string ip = args[1];
                    int port = int.Parse(args[2]);
                    string name = args[3];
                    Console.WriteLine(SessionLauncher.BuildJoinArgs(ip, port, name));
                    return 0;
                }

                if (cmd == "--test-lobby")
                {
                    Console.WriteLine("Running automated lobby discovery test on port 27015...");
                    using (LobbyBeaconService service = new LobbyBeaconService())
                    {
                        bool detected = false;
                        service.OnServerUpdated += (srv) =>
                        {
                            if (srv.ServerName == "AutomatedTestServer")
                            {
                                Console.WriteLine("Discovered server: " + srv.DisplayText);
                                detected = true;
                            }
                        };

                        service.StartListener();

                        LobbyServerInfo testServer = new LobbyServerInfo
                        {
                            ServerName = "AutomatedTestServer",
                            HostName = "TestHost",
                            IP = "127.0.0.1",
                            Port = 27016,
                            Level = "marsh",
                            CurrentPlayers = 1,
                            MaxPlayers = 4
                        };

                        service.StartBroadcaster(testServer);

                        // Wait up to 3 seconds for broadcast to arrive
                        for (int i = 0; i < 30; i++)
                        {
                            if (detected) break;
                            Thread.Sleep(100);
                        }

                        service.StopBroadcaster();

                        if (detected)
                        {
                            Console.WriteLine("LOBBY DISCOVERY TEST PASSED");
                            return 0;
                        }
                        else
                        {
                            Console.Error.WriteLine("LOBBY DISCOVERY TIMEOUT");
                            return 1;
                        }
                    }
                }
            }

            // Normal GUI launch
            try
            {
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new MainForm());
                return 0;
            }
            catch (Exception ex)
            {
                string crashLog = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "launcher_crash.log");
                File.WriteAllText(crashLog, ex.ToString());
                Console.Error.WriteLine("LAUNCHER CRASH: " + ex);
                MessageBox.Show(ex.ToString(), "Launcher Crash Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return 1;
            }
        }
    }
}
