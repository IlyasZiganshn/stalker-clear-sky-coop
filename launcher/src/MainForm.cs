using System;
using System.IO;
using System.Drawing;
using System.Windows.Forms;
using System.Diagnostics;
using System.Collections.Generic;

namespace ClearSkyCoop.Launcher
{
    public class MainForm : Form
    {
        private LobbyBeaconService _beaconService;
        private string _gamePath;

        // UI Controls
        private TabControl _tabControl;
        private TabPage _tabLobby;
        private TabPage _tabHost;
        private TabPage _tabSettings;
        private TabPage _tabDiagnostics;

        // Lobby Tab
        private ListView _lvServers;
        private TextBox _tbDirectIp;
        private TextBox _tbDirectPort;
        private TextBox _tbPlayerName;
        private Button _btnJoinSelected;
        private Button _btnDirectConnect;
        private Label _lblLobbyStatus;

        // Host Tab
        private TextBox _tbHostServerName;
        private TextBox _tbHostPlayerName;
        private NumericUpDown _nudHostPort;
        private Button _btnStartHost;
        private Label _lblHostStatus;

        // Settings Tab
        private TextBox _tbGamePath;
        private Button _btnBrowseGame;
        private Label _lblModStatus;
        private Button _btnDeployMod;
        private CheckBox _cbSkipIntro;

        // Diagnostics Tab
        private Button _btnCollectDiag;
        private TextBox _tbDiagNotes;
        private Label _lblDiagStatus;

        // Status bar
        private StatusStrip _statusStrip;
        private ToolStripStatusLabel _statusLabel;

        public MainForm()
        {
            InitializeComponents();
            InitializeLogic();
        }

        private void InitializeComponents()
        {
            this.Text = "S.T.A.L.K.E.R. Clear Sky - Co-op Launcher";
            this.Size = new Size(720, 500);
            this.MinimumSize = new Size(640, 440);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Color.FromArgb(32, 34, 37);
            this.ForeColor = Color.WhiteSmoke;
            this.Font = new Font("Segoe UI", 9.5f, FontStyle.Regular);

            // Tab Control
            _tabControl = new TabControl();
            _tabControl.Dock = DockStyle.Fill;
            _tabControl.Padding = new Point(12, 6);

            _tabLobby = new TabPage("Lobby / Join");
            _tabHost = new TabPage("Host Game");
            _tabSettings = new TabPage("Settings");
            _tabDiagnostics = new TabPage("Diagnostics");

            _tabLobby.BackColor = Color.FromArgb(40, 42, 46);
            _tabHost.BackColor = Color.FromArgb(40, 42, 46);
            _tabSettings.BackColor = Color.FromArgb(40, 42, 46);
            _tabDiagnostics.BackColor = Color.FromArgb(40, 42, 46);

            BuildLobbyTab();
            BuildHostTab();
            BuildSettingsTab();
            BuildDiagnosticsTab();

            _tabControl.TabPages.Add(_tabLobby);
            _tabControl.TabPages.Add(_tabHost);
            _tabControl.TabPages.Add(_tabSettings);
            _tabControl.TabPages.Add(_tabDiagnostics);

            // Status bar
            _statusStrip = new StatusStrip();
            _statusStrip.BackColor = Color.FromArgb(24, 25, 28);
            _statusStrip.ForeColor = Color.LightGray;
            _statusLabel = new ToolStripStatusLabel("Ready");
            _statusStrip.Items.Add(_statusLabel);

            this.Controls.Add(_tabControl);
            this.Controls.Add(_statusStrip);
        }

        private void BuildLobbyTab()
        {
            TableLayoutPanel panel = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(10),
                RowCount = 4,
                ColumnCount = 1
            };
            panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 35));
            panel.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 45));
            panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));

            // Top player name row
            FlowLayoutPanel namePanel = new FlowLayoutPanel { Dock = DockStyle.Fill };
            Label lblName = new Label { Text = "Your Name:", AutoSize = true, Padding = new Padding(0, 5, 5, 0) };
            _tbPlayerName = new TextBox
            {
                Text = Environment.UserName,
                Width = 160,
                BackColor = Color.FromArgb(50, 52, 56),
                ForeColor = Color.White
            };
            namePanel.Controls.Add(lblName);
            namePanel.Controls.Add(_tbPlayerName);

            // Server ListView
            _lvServers = new ListView
            {
                Dock = DockStyle.Fill,
                View = View.Details,
                FullRowSelect = true,
                GridLines = true,
                BackColor = Color.FromArgb(48, 50, 54),
                ForeColor = Color.White
            };
            _lvServers.Columns.Add("Server Name", 180);
            _lvServers.Columns.Add("Host", 110);
            _lvServers.Columns.Add("Level", 100);
            _lvServers.Columns.Add("Players", 70);
            _lvServers.Columns.Add("Address", 140);
            _lvServers.DoubleClick += (s, e) => JoinSelectedServer();

            // Direct Connect row
            FlowLayoutPanel directPanel = new FlowLayoutPanel { Dock = DockStyle.Fill };
            Label lblDirect = new Label { Text = "Direct IP:", AutoSize = true, Padding = new Padding(0, 5, 5, 0) };
            _tbDirectIp = new TextBox { Text = "127.0.0.1", Width = 140, BackColor = Color.FromArgb(50, 52, 56), ForeColor = Color.White };
            Label lblPort = new Label { Text = "Port:", AutoSize = true, Padding = new Padding(10, 5, 5, 0) };
            _tbDirectPort = new TextBox { Text = "27016", Width = 60, BackColor = Color.FromArgb(50, 52, 56), ForeColor = Color.White };
            _btnDirectConnect = new Button
            {
                Text = "Connect Direct",
                Width = 120,
                Height = 28,
                BackColor = Color.FromArgb(70, 75, 85),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat
            };
            _btnDirectConnect.Click += (s, e) => JoinDirectServer();

            _btnJoinSelected = new Button
            {
                Text = "Join Selected",
                Width = 130,
                Height = 28,
                BackColor = Color.FromArgb(46, 125, 50),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat
            };
            _btnJoinSelected.Click += (s, e) => JoinSelectedServer();

            directPanel.Controls.Add(lblDirect);
            directPanel.Controls.Add(_tbDirectIp);
            directPanel.Controls.Add(lblPort);
            directPanel.Controls.Add(_tbDirectPort);
            directPanel.Controls.Add(_btnDirectConnect);
            directPanel.Controls.Add(_btnJoinSelected);

            _lblLobbyStatus = new Label
            {
                Text = "Listening for LAN co-op lobbies on port 27015...",
                Dock = DockStyle.Fill,
                ForeColor = Color.LightSteelBlue
            };

            panel.Controls.Add(namePanel, 0, 0);
            panel.Controls.Add(_lvServers, 0, 1);
            panel.Controls.Add(directPanel, 0, 2);
            panel.Controls.Add(_lblLobbyStatus, 0, 3);

            _tabLobby.Controls.Add(panel);
        }

        private void BuildHostTab()
        {
            TableLayoutPanel panel = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(25),
                RowCount = 6,
                ColumnCount = 2
            };
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 140));
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));

            for (int i = 0; i < 5; i++) panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
            panel.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

            // Server Name
            panel.Controls.Add(new Label { Text = "Server Name:", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, 0, 0);
            _tbHostServerName = new TextBox
            {
                Text = Environment.UserName + "'s Clear Sky Campaign",
                Width = 320,
                BackColor = Color.FromArgb(50, 52, 56),
                ForeColor = Color.White
            };
            panel.Controls.Add(_tbHostServerName, 1, 0);

            // Player Name
            panel.Controls.Add(new Label { Text = "Host Player Name:", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, 0, 1);
            _tbHostPlayerName = new TextBox
            {
                Text = Environment.UserName,
                Width = 200,
                BackColor = Color.FromArgb(50, 52, 56),
                ForeColor = Color.White
            };
            panel.Controls.Add(_tbHostPlayerName, 1, 1);

            // Port
            panel.Controls.Add(new Label { Text = "Game Port (UDP):", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, 0, 2);
            _nudHostPort = new NumericUpDown
            {
                Minimum = 1024,
                Maximum = 65535,
                Value = 27016,
                Width = 100,
                BackColor = Color.FromArgb(50, 52, 56),
                ForeColor = Color.White
            };
            panel.Controls.Add(_nudHostPort, 1, 2);

            // Start Host Button
            _btnStartHost = new Button
            {
                Text = "Start Co-op Host",
                Width = 180,
                Height = 35,
                BackColor = Color.FromArgb(46, 125, 50),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat,
                Font = new Font("Segoe UI", 10.5f, FontStyle.Bold)
            };
            _btnStartHost.Click += (s, e) => StartHostingSession();
            panel.Controls.Add(_btnStartHost, 1, 3);

            _lblHostStatus = new Label
            {
                Text = "Clicking Start Host launches Clear Sky and broadcasts lobby on LAN automatically.",
                AutoSize = true,
                ForeColor = Color.LightGray,
                Padding = new Padding(0, 8, 0, 0)
            };
            panel.Controls.Add(_lblHostStatus, 1, 4);

            _tabHost.Controls.Add(panel);
        }

        private void BuildSettingsTab()
        {
            TableLayoutPanel panel = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(20),
                RowCount = 5,
                ColumnCount = 2
            };
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 130));
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));

            panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 45));
            panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
            panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 45));
            panel.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

            // Game Path
            panel.Controls.Add(new Label { Text = "Game Path:", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, 0, 0);
            FlowLayoutPanel pathRow = new FlowLayoutPanel { Dock = DockStyle.Fill };
            _tbGamePath = new TextBox { Width = 380, BackColor = Color.FromArgb(50, 52, 56), ForeColor = Color.White };
            _tbGamePath.TextChanged += (s, e) => UpdateModStatus();
            _btnBrowseGame = new Button
            {
                Text = "Browse...",
                Width = 90,
                Height = 26,
                BackColor = Color.FromArgb(70, 75, 85),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat
            };
            _btnBrowseGame.Click += (s, e) => BrowseGamePath();
            pathRow.Controls.Add(_tbGamePath);
            pathRow.Controls.Add(_btnBrowseGame);
            panel.Controls.Add(pathRow, 1, 0);

            // Mod Status
            panel.Controls.Add(new Label { Text = "Mod Status:", AutoSize = true, Padding = new Padding(0, 6, 0, 0) }, 0, 1);
            _lblModStatus = new Label { Text = "Checking...", AutoSize = true, Padding = new Padding(0, 6, 0, 0) };
            panel.Controls.Add(_lblModStatus, 1, 1);

            // Deploy button
            _btnDeployMod = new Button
            {
                Text = "Install / Deploy Mod Files",
                Width = 200,
                Height = 30,
                BackColor = Color.FromArgb(30, 136, 229),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat
            };
            _btnDeployMod.Click += (s, e) => DeployModFiles();
            panel.Controls.Add(_btnDeployMod, 1, 2);

            // Skip Intro Checkbox
            _cbSkipIntro = new CheckBox
            {
                Text = "Skip introductory story / cutscene (-nogameintro)",
                AutoSize = true,
                Checked = false,
                ForeColor = Color.LightGray,
                Padding = new Padding(0, 10, 0, 0)
            };
            panel.Controls.Add(_cbSkipIntro, 1, 3);

            _tabSettings.Controls.Add(panel);
        }

        private void BuildDiagnosticsTab()
        {
            TableLayoutPanel panel = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(20),
                RowCount = 5,
                ColumnCount = 1
            };
            panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 45));
            panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 70));
            panel.RowStyles.Add(new RowStyle(SizeType.Absolute, 45));
            panel.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

            Label lblTitle = new Label
            {
                Text = "One-Click Diagnostic Pack Generator",
                Font = new Font("Segoe UI", 11f, FontStyle.Bold),
                AutoSize = true
            };
            panel.Controls.Add(lblTitle, 0, 0);

            Label lblDesc = new Label
            {
                Text = "Collects engine logs, crash dumps, savegames, user configs, and system details into a single ZIP archive for fast bug reporting and 1-run reproduction.",
                Dock = DockStyle.Fill,
                ForeColor = Color.LightGray
            };
            panel.Controls.Add(lblDesc, 0, 1);

            FlowLayoutPanel btnRow = new FlowLayoutPanel { Dock = DockStyle.Fill };
            _btnCollectDiag = new Button
            {
                Text = "Generate Diagnostic ZIP",
                Width = 200,
                Height = 32,
                BackColor = Color.FromArgb(92, 107, 115),
                ForeColor = Color.White,
                FlatStyle = FlatStyle.Flat
            };
            _btnCollectDiag.Click += (s, e) => CollectDiagnostics();
            _lblDiagStatus = new Label { Text = "", AutoSize = true, Padding = new Padding(10, 8, 0, 0), ForeColor = Color.LimeGreen };
            btnRow.Controls.Add(_btnCollectDiag);
            btnRow.Controls.Add(_lblDiagStatus);
            panel.Controls.Add(btnRow, 0, 2);

            _tabDiagnostics.Controls.Add(panel);
        }

        private void InitializeLogic()
        {
            // Auto-detect game
            _gamePath = GameLocator.DetectGamePath();
            if (!string.IsNullOrEmpty(_gamePath))
            {
                _tbGamePath.Text = _gamePath;
                UpdateModStatus();
                _statusLabel.Text = "Game detected: " + _gamePath;
            }
            else
            {
                _statusLabel.Text = "Game not automatically detected. Please specify path in Settings.";
            }

            // Start LAN Lobby Beacon listener
            _beaconService = new LobbyBeaconService();
            _beaconService.OnServerUpdated += OnLobbyServerUpdated;
            _beaconService.OnServerRemoved += OnLobbyServerRemoved;
            _beaconService.StartListener();
        }

        private void UpdateModStatus()
        {
            string norm = ModDeployer.NormalizeGamePath(_tbGamePath.Text);
            if (string.IsNullOrEmpty(norm))
            {
                _lblModStatus.Text = "No game folder selected.";
                _lblModStatus.ForeColor = Color.OrangeRed;
                return;
            }

            if (ModDeployer.IsModInstalled(norm))
            {
                _lblModStatus.Text = "Installed and ready (OpenXRay x64 binaries present)";
                _lblModStatus.ForeColor = Color.LightGreen;
            }
            else
            {
                _lblModStatus.Text = "Mod binaries missing in game folder. Click 'Install / Deploy Mod Files' below.";
                _lblModStatus.ForeColor = Color.Orange;
            }
        }

        private void OnLobbyServerUpdated(LobbyServerInfo server)
        {
            if (this.InvokeRequired)
            {
                this.BeginInvoke(new Action<LobbyServerInfo>(OnLobbyServerUpdated), server);
                return;
            }

            string key = server.IP + ":" + server.Port;
            ListViewItem item = null;
            foreach (ListViewItem existing in _lvServers.Items)
            {
                if (existing.Tag as string == key)
                {
                    item = existing;
                    break;
                }
            }

            if (item == null)
            {
                item = new ListViewItem(server.ServerName);
                item.SubItems.Add(server.HostName);
                item.SubItems.Add(server.Level);
                item.SubItems.Add(string.Format("{0}/{1}", server.CurrentPlayers, server.MaxPlayers));
                item.SubItems.Add(key);
                item.Tag = key;
                _lvServers.Items.Add(item);
            }
            else
            {
                item.Text = server.ServerName;
                item.SubItems[1].Text = server.HostName;
                item.SubItems[2].Text = server.Level;
                item.SubItems[3].Text = string.Format("{0}/{1}", server.CurrentPlayers, server.MaxPlayers);
            }
        }

        private void OnLobbyServerRemoved(string key)
        {
            if (this.InvokeRequired)
            {
                this.BeginInvoke(new Action<string>(OnLobbyServerRemoved), key);
                return;
            }

            for (int i = _lvServers.Items.Count - 1; i >= 0; i--)
            {
                if (_lvServers.Items[i].Tag as string == key)
                {
                    _lvServers.Items.RemoveAt(i);
                }
            }
        }

        private void JoinSelectedServer()
        {
            if (_lvServers.SelectedItems.Count == 0)
            {
                MessageBox.Show("Please select a server from the lobby list, or use Direct Connect.",
                    "No Server Selected", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            ListViewItem item = _lvServers.SelectedItems[0];
            string address = item.Tag as string;
            if (string.IsNullOrEmpty(address)) return;

            string[] parts = address.Split(':');
            string ip = parts[0];
            int port = int.Parse(parts[1]);

            LaunchClient(ip, port);
        }

        private void JoinDirectServer()
        {
            string ip = _tbDirectIp.Text.Trim();
            int port;
            if (!int.TryParse(_tbDirectPort.Text.Trim(), out port)) port = 27016;

            if (string.IsNullOrEmpty(ip))
            {
                MessageBox.Show("Please enter a valid IP address.", "Invalid IP", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            LaunchClient(ip, port);
        }

        private void LaunchClient(string ip, int port)
        {
            string gamePath = _tbGamePath.Text;
            if (!ModDeployer.IsModInstalled(gamePath))
            {
                MessageBox.Show("Mod files are not installed in the game directory. Please install them first under Settings tab.",
                    "Mod Not Installed", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                _tabControl.SelectedTab = _tabSettings;
                return;
            }

            string playerName = _tbPlayerName.Text.Trim();
            string error;
            bool skipIntro = _cbSkipIntro != null && _cbSkipIntro.Checked;
            Process proc = SessionLauncher.StartClient(gamePath, ip, port, playerName, skipIntro, out error);
            if (proc != null)
            {
                _statusLabel.Text = string.Format("Connecting to {0}:{1} as '{2}'...", ip, port, playerName);
            }
            else
            {
                MessageBox.Show("Failed to launch client: " + error, "Launch Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void StartHostingSession()
        {
            string gamePath = _tbGamePath.Text;
            if (!ModDeployer.IsModInstalled(gamePath))
            {
                MessageBox.Show("Mod files are not installed in the game directory. Please install them first under Settings tab.",
                    "Mod Not Installed", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                _tabControl.SelectedTab = _tabSettings;
                return;
            }

            int port = (int)_nudHostPort.Value;
            string serverName = _tbHostServerName.Text.Trim();
            string hostName = _tbHostPlayerName.Text.Trim();

            string error;
            bool skipIntro = _cbSkipIntro != null && _cbSkipIntro.Checked;
            Process proc = SessionLauncher.StartHost(gamePath, port, skipIntro, out error);
            if (proc != null)
            {
                _statusLabel.Text = string.Format("Host running on port {0}. Broadcasting lobby beacon...", port);

                // Start advertising on LAN
                LobbyServerInfo hostInfo = new LobbyServerInfo
                {
                    ServerName = serverName,
                    HostName = hostName,
                    Port = port,
                    Level = "marsh",
                    CurrentPlayers = 1,
                    MaxPlayers = 4
                };
                _beaconService.StartBroadcaster(hostInfo);
                _lblHostStatus.Text = string.Format("Hosting active on port {0}. Beacon broadcast is active.", port);
                _lblHostStatus.ForeColor = Color.LightGreen;
            }
            else
            {
                MessageBox.Show("Failed to start host: " + error, "Launch Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void BrowseGamePath()
        {
            using (FolderBrowserDialog fbd = new FolderBrowserDialog())
            {
                fbd.Description = "Select S.T.A.L.K.E.R. Clear Sky installation folder (containing fsgame.ltx)";
                if (fbd.ShowDialog() == DialogResult.OK)
                {
                    _tbGamePath.Text = ModDeployer.NormalizeGamePath(fbd.SelectedPath);
                    UpdateModStatus();
                }
            }
        }

        private void DeployModFiles()
        {
            string raw = _tbGamePath.Text;
            string norm = ModDeployer.NormalizeGamePath(raw);
            if (!string.IsNullOrEmpty(norm) && norm != raw)
            {
                _tbGamePath.Text = norm;
            }

            string error;
            if (ModDeployer.DeployMod(_tbGamePath.Text, out error))
            {
                MessageBox.Show("Mod binaries, configs, and scripts successfully deployed to game folder!",
                    "Deploy Success", MessageBoxButtons.OK, MessageBoxIcon.Information);
                UpdateModStatus();
            }
            else
            {
                MessageBox.Show("Failed to deploy mod:\n\n" + error, "Deploy Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void CollectDiagnostics()
        {
            try
            {
                _lblDiagStatus.Text = "Collecting diagnostics...";
                string scriptPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, @"scripts\collect_diagnostics.ps1");
                if (!File.Exists(scriptPath))
                {
                    // Search in parent
                    string root = Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, @"..\.."));
                    scriptPath = Path.Combine(root, @"scripts\collect_diagnostics.ps1");
                }

                if (File.Exists(scriptPath))
                {
                    ProcessStartInfo psi = new ProcessStartInfo
                    {
                        FileName = "powershell.exe",
                        Arguments = string.Format("-NoProfile -ExecutionPolicy Bypass -File \"{0}\" -Zip", scriptPath),
                        UseShellExecute = false,
                        CreateNoWindow = true
                    };
                    Process p = Process.Start(psi);
                    p.WaitForExit();
                    _lblDiagStatus.Text = "Diagnostic pack generated in diagnostics/ folder!";
                }
                else
                {
                    _lblDiagStatus.Text = "Diagnostic script not found at " + scriptPath;
                }
            }
            catch (Exception ex)
            {
                _lblDiagStatus.Text = "Error: " + ex.Message;
            }
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            if (_beaconService != null)
            {
                _beaconService.Dispose();
                _beaconService = null;
            }
            base.OnFormClosing(e);
        }
    }
}
