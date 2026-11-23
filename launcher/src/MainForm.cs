using System;
using System.IO;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
using System.Diagnostics;
using System.Collections.Generic;
using System.Reflection;

namespace ClearSkyCoop.Launcher
{
    public class MainForm : Form
    {
        private LobbyBeaconService _beaconService;
        private string _gamePath;
        private string _localIp = "127.0.0.1";

        // Custom Titlebar
        private TitleBarControl _titleBar;

        // Navigation Sidebar
        private Panel _sidebar;
        private ModernButton _btnNavLobby;
        private ModernButton _btnNavHost;
        private ModernButton _btnNavSettings;
        private ModernButton _btnNavDiag;
        private Label _lblSidebarPlayer;
        private Label _lblSidebarIp;

        // Content Area & Pages
        private Panel _contentPanel;
        private Panel _topBar;
        private Label _lblPageTitle;
        private Label _lblNetworkBadge;
        private Label _lblModBadge;

        private Panel _pageLobby;
        private Panel _pageHost;
        private Panel _pageSettings;
        private Panel _pageDiag;
        private Panel[] _pages;
        private ModernButton[] _navButtons;

        // Page 1: Lobby Controls
        private ListView _lvServers;
        private ModernTextBox _tbPlayerName;
        private ModernTextBox _tbDirectIp;
        private ModernTextBox _tbDirectPort;
        private ModernButton _btnJoinSelected;
        private ModernButton _btnDirectConnect;
        private ModernButton _btnRefreshLobby;
        private Label _lblLobbyEmpty;

        // Page 2: Host Controls
        private ModernTextBox _tbHostServerName;
        private ModernTextBox _tbHostPlayerName;
        private NumericUpDown _nudHostPort;
        private CheckBox _cbHostBroadcast;
        private CheckBox _cbHostSkipIntro;
        private ModernButton _btnStartHost;
        private Label _lblHostLiveStatus;
        private Label _lblHostAddressCopy;

        // Page 3: Settings Controls
        private ModernTextBox _tbGamePath;
        private ModernButton _btnBrowse;
        private ModernButton _btnDeployMod;
        private Label _lblSettingsModStatus;
        private CheckBox _cbGlobalSkipIntro;
        private ModernButton _btnOpenGameFolder;
        private ModernButton _btnOpenLogsFolder;
        private ModernButton _btnRedetect;

        // Page 4: Diagnostics Controls
        private ModernButton _btnGenerateDiag;
        private ModernButton _btnOpenDiagFolder;
        private Label _lblDiagStatus;

        public MainForm()
        {
            this.SetStyle(ControlStyles.AllPaintingInWmPaint |
                           ControlStyles.UserPaint |
                           ControlStyles.OptimizedDoubleBuffer |
                           ControlStyles.ResizeRedraw, true);

            InitializeForm();
            InitializeComponents();
            InitializeLogic();
        }

        private void InitializeForm()
        {
            this.Text = "S.T.A.L.K.E.R. Clear Sky - Co-op Launcher";
            this.FormBorderStyle = FormBorderStyle.None;
            this.Size = new Size(960, 600);
            this.MinimumSize = new Size(900, 540);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Theme.BgDarkest;
            this.ForeColor = Theme.TextPrimary;
            this.Font = Theme.FontRegular;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            // Draw subtle 1px border around frameless window
            using (Pen borderPen = new Pen(Theme.Border, 1f))
            {
                e.Graphics.DrawRectangle(borderPen, 0, 0, Width - 1, Height - 1);
            }
        }

        private void InitializeComponents()
        {
            // 1. Title bar
            _titleBar = new TitleBarControl(this);
            this.Controls.Add(_titleBar);

            // 2. Sidebar
            BuildSidebar();

            // 3. Right Content Container
            _contentPanel = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest,
                Padding = new Padding(20, 16, 20, 16)
            };

            // Top Status Bar in Content area
            BuildTopBar();

            // Build individual tab pages
            BuildLobbyPage();
            BuildHostPage();
            BuildSettingsPage();
            BuildDiagnosticsPage();

            _pages = new Panel[] { _pageLobby, _pageHost, _pageSettings, _pageDiag };
            _navButtons = new ModernButton[] { _btnNavLobby, _btnNavHost, _btnNavSettings, _btnNavDiag };

            foreach (Panel p in _pages)
            {
                _contentPanel.Controls.Add(p);
            }

            this.Controls.Add(_contentPanel);
            this.Controls.Add(_sidebar);

            SwitchPage(0);
        }

        private void BuildSidebar()
        {
            _sidebar = new Panel
            {
                Dock = DockStyle.Left,
                Width = 220,
                BackColor = Theme.BgSidebar
            };

            // Top Faction Logo / Header
            Panel headerPanel = new Panel
            {
                Dock = DockStyle.Top,
                Height = 85,
                BackColor = Theme.BgSidebar,
                Padding = new Padding(18, 16, 18, 10)
            };

            Label lblFaction = new Label
            {
                Text = "ЧИСТОЕ НЕБО",
                Font = new Font("Segoe UI", 12f, FontStyle.Bold),
                ForeColor = Theme.TextPrimary,
                Dock = DockStyle.Top,
                Height = 26
            };
            Label lblSub = new Label
            {
                Text = "MULTIPLAYER CO-OP",
                Font = new Font("Segoe UI", 8f, FontStyle.Bold),
                ForeColor = Theme.AccentCyan,
                Dock = DockStyle.Top,
                Height = 18
            };
            Panel divider = new Panel
            {
                Dock = DockStyle.Bottom,
                Height = 1,
                BackColor = Theme.Border
            };

            headerPanel.Controls.Add(lblSub);
            headerPanel.Controls.Add(lblFaction);
            headerPanel.Controls.Add(divider);
            _sidebar.Controls.Add(headerPanel);

            // Nav Buttons Container
            Panel navContainer = new Panel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(10, 14, 10, 10)
            };

            _btnNavLobby = CreateNavButton("🌐   Lobby & Join", 0);
            _btnNavHost = CreateNavButton("⚡   Host Server", 1);
            _btnNavSettings = CreateNavButton("⚙️   Settings & Mod", 2);
            _btnNavDiag = CreateNavButton("🛠️   Diagnostics", 3);

            _btnNavLobby.Top = 14;
            _btnNavHost.Top = 60;
            _btnNavSettings.Top = 106;
            _btnNavDiag.Top = 152;

            navContainer.Controls.Add(_btnNavLobby);
            navContainer.Controls.Add(_btnNavHost);
            navContainer.Controls.Add(_btnNavSettings);
            navContainer.Controls.Add(_btnNavDiag);
            _sidebar.Controls.Add(navContainer);

            // Bottom Profile / Network Card
            Panel bottomProfile = new Panel
            {
                Dock = DockStyle.Bottom,
                Height = 80,
                BackColor = Color.FromArgb(16, 19, 24),
                Padding = new Padding(16, 12, 16, 12)
            };

            _lblSidebarPlayer = new Label
            {
                Text = "👤 " + Environment.UserName,
                Font = Theme.FontRegularBold,
                ForeColor = Theme.TextPrimary,
                Dock = DockStyle.Top,
                Height = 22
            };
            _lblSidebarIp = new Label
            {
                Text = "LAN: " + _localIp,
                Font = Theme.FontSmall,
                ForeColor = Theme.TextSecondary,
                Dock = DockStyle.Top,
                Height = 18
            };

            bottomProfile.Controls.Add(_lblSidebarIp);
            bottomProfile.Controls.Add(_lblSidebarPlayer);
            _sidebar.Controls.Add(bottomProfile);
        }

        private ModernButton CreateNavButton(string text, int pageIndex)
        {
            ModernButton btn = new ModernButton
            {
                Text = text,
                Variant = ButtonVariant.Nav,
                Width = 200,
                Height = 40,
                Font = Theme.FontRegularBold
            };
            btn.Click += (s, e) => SwitchPage(pageIndex);
            return btn;
        }

        private void BuildTopBar()
        {
            _topBar = new Panel
            {
                Dock = DockStyle.Top,
                Height = 42,
                BackColor = Theme.BgDarkest
            };

            _lblPageTitle = new Label
            {
                Text = "LOBBY & SERVER BROWSER",
                Font = Theme.FontTitle,
                ForeColor = Theme.TextPrimary,
                AutoSize = true,
                Location = new Point(0, 8)
            };

            FlowLayoutPanel badgePanel = new FlowLayoutPanel
            {
                Dock = DockStyle.Right,
                AutoSize = true,
                FlowDirection = FlowDirection.LeftToRight
            };

            _lblNetworkBadge = new Label
            {
                Text = "● LAN Beacon: 27015",
                Font = Theme.FontBadge,
                ForeColor = Theme.AccentGreen,
                BackColor = Color.FromArgb(20, 45, 35),
                Padding = new Padding(8, 4, 8, 4),
                Margin = new Padding(0, 6, 8, 0),
                AutoSize = true
            };

            _lblModBadge = new Label
            {
                Text = "Checking Mod...",
                Font = Theme.FontBadge,
                ForeColor = Theme.AccentAmber,
                BackColor = Color.FromArgb(45, 38, 20),
                Padding = new Padding(8, 4, 8, 4),
                Margin = new Padding(0, 6, 0, 0),
                AutoSize = true
            };

            badgePanel.Controls.Add(_lblNetworkBadge);
            badgePanel.Controls.Add(_lblModBadge);

            _topBar.Controls.Add(_lblPageTitle);
            _topBar.Controls.Add(badgePanel);
            _contentPanel.Controls.Add(_topBar);
        }

        private void SwitchPage(int index)
        {
            for (int i = 0; i < _pages.Length; i++)
            {
                bool active = (i == index);
                _pages[i].Visible = active;
                _navButtons[i].IsActiveNav = active;
            }

            switch (index)
            {
                case 0: _lblPageTitle.Text = "LOBBY & SERVER BROWSER"; break;
                case 1: _lblPageTitle.Text = "HOST CO-OP CAMPAIGN"; break;
                case 2: _lblPageTitle.Text = "SETTINGS & MOD INSTALLATION"; break;
                case 3: _lblPageTitle.Text = "DIAGNOSTICS & SYSTEM REPORT"; break;
            }
        }

        private void BuildLobbyPage()
        {
            _pageLobby = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest,
                Padding = new Padding(0, 10, 0, 0)
            };

            // Main Server List Card
            ModernCard serverCard = new ModernCard
            {
                Dock = DockStyle.Fill,
                Title = "Available LAN Co-op Sessions",
                Subtitle = "Servers broadcasting on UDP port 27015"
            };

            // Server ListView
            _lvServers = new ListView
            {
                View = View.Details,
                FullRowSelect = true,
                GridLines = false,
                BorderStyle = BorderStyle.None,
                BackColor = Theme.BgCard,
                ForeColor = Theme.TextPrimary,
                Font = Theme.FontRegular,
                Dock = DockStyle.Fill
            };
            EnableDoubleBuffer(_lvServers);

            _lvServers.Columns.Add("Server Name", 240);
            _lvServers.Columns.Add("Host Stalker", 130);
            _lvServers.Columns.Add("Location", 120);
            _lvServers.Columns.Add("Players", 80);
            _lvServers.Columns.Add("Address (IP:Port)", 140);
            _lvServers.DoubleClick += (s, e) => JoinSelectedServer();

            _lblLobbyEmpty = new Label
            {
                Text = "Listening for co-op games on your local network...\n\nHost a game on PC 1, or use Direct Connect below.",
                Font = Theme.FontRegular,
                ForeColor = Theme.TextMuted,
                TextAlign = ContentAlignment.MiddleCenter,
                Dock = DockStyle.Fill,
                BackColor = Theme.BgCard
            };

            Panel listContainer = new Panel { Dock = DockStyle.Fill, Padding = new Padding(0, 48, 0, 50) };
            listContainer.Controls.Add(_lblLobbyEmpty);
            listContainer.Controls.Add(_lvServers);

            // Action row below list
            Panel listActionRow = new Panel
            {
                Dock = DockStyle.Bottom,
                Height = 44,
                Padding = new Padding(0, 6, 0, 0)
            };

            Label lblNick = new Label
            {
                Text = "Your Name:",
                Font = Theme.FontRegularBold,
                ForeColor = Theme.TextSecondary,
                AutoSize = true,
                Location = new Point(0, 12)
            };

            _tbPlayerName = new ModernTextBox
            {
                Text = Environment.UserName,
                Width = 160,
                Height = 34,
                Location = new Point(85, 4)
            };

            _btnRefreshLobby = new ModernButton
            {
                Text = "🔄 Refresh",
                Width = 100,
                Height = 34,
                Location = new Point(255, 4),
                Variant = ButtonVariant.Secondary
            };
            _btnRefreshLobby.Click += (s, e) => RefreshLobbyServers();

            _btnJoinSelected = new ModernButton
            {
                Text = "▶   JOIN SELECTED SERVER",
                Width = 220,
                Height = 34,
                Anchor = AnchorStyles.Top | AnchorStyles.Right,
                Location = new Point(serverCard.Width - 236, 4),
                Variant = ButtonVariant.PrimaryCyan,
                Font = Theme.FontSubtitle
            };
            _btnJoinSelected.Click += (s, e) => JoinSelectedServer();

            listActionRow.Controls.Add(lblNick);
            listActionRow.Controls.Add(_tbPlayerName);
            listActionRow.Controls.Add(_btnRefreshLobby);
            listActionRow.Controls.Add(_btnJoinSelected);

            serverCard.Controls.Add(listContainer);
            serverCard.Controls.Add(listActionRow);

            // Bottom Direct Connect Card
            ModernCard directCard = new ModernCard
            {
                Dock = DockStyle.Bottom,
                Height = 100,
                Title = "Direct IP Connection",
                Subtitle = "Connect directly to a server by IP and port"
            };

            FlowLayoutPanel directInputs = new FlowLayoutPanel
            {
                Dock = DockStyle.Bottom,
                Height = 46,
                Padding = new Padding(0, 6, 0, 0)
            };

            Label lblIp = new Label { Text = "IP:", Font = Theme.FontRegularBold, ForeColor = Theme.TextSecondary, AutoSize = true, Margin = new Padding(0, 8, 4, 0) };
            _tbDirectIp = new ModernTextBox { Text = "127.0.0.1", Width = 150, Height = 32 };
            Label lblPort = new Label { Text = "Port:", Font = Theme.FontRegularBold, ForeColor = Theme.TextSecondary, AutoSize = true, Margin = new Padding(12, 8, 4, 0) };
            _tbDirectPort = new ModernTextBox { Text = "27016", Width = 70, Height = 32 };

            _btnDirectConnect = new ModernButton
            {
                Text = "⚡ Connect Direct",
                Width = 150,
                Height = 32,
                Margin = new Padding(16, 0, 0, 0),
                Variant = ButtonVariant.Secondary
            };
            _btnDirectConnect.Click += (s, e) => JoinDirectServer();

            directInputs.Controls.Add(lblIp);
            directInputs.Controls.Add(_tbDirectIp);
            directInputs.Controls.Add(lblPort);
            directInputs.Controls.Add(_tbDirectPort);
            directInputs.Controls.Add(_btnDirectConnect);

            directCard.Controls.Add(directInputs);

            _pageLobby.Controls.Add(serverCard);
            _pageLobby.Controls.Add(directCard);
        }

        private void BuildHostPage()
        {
            _pageHost = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest,
                Padding = new Padding(0, 10, 0, 0)
            };

            ModernCard configCard = new ModernCard
            {
                Dock = DockStyle.Fill,
                Title = "Campaign Host Configuration",
                Subtitle = "Start a synchronized cooperative story campaign"
            };

            TableLayoutPanel table = new TableLayoutPanel
            {
                Dock = DockStyle.Top,
                Height = 240,
                RowCount = 5,
                ColumnCount = 2,
                Padding = new Padding(0, 48, 0, 0)
            };
            table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 160));
            table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));

            for (int i = 0; i < 4; i++) table.RowStyles.Add(new RowStyle(SizeType.Absolute, 42));
            table.RowStyles.Add(new RowStyle(SizeType.Absolute, 45));

            // Row 0: Server Name
            table.Controls.Add(new Label { Text = "Session Name:", Font = Theme.FontRegularBold, ForeColor = Theme.TextSecondary, Padding = new Padding(0, 6, 0, 0) }, 0, 0);
            _tbHostServerName = new ModernTextBox
            {
                Text = Environment.UserName + "'s Clear Sky Campaign",
                Width = 360,
                Height = 32
            };
            table.Controls.Add(_tbHostServerName, 1, 0);

            // Row 1: Host Name
            table.Controls.Add(new Label { Text = "Host Nickname:", Font = Theme.FontRegularBold, ForeColor = Theme.TextSecondary, Padding = new Padding(0, 6, 0, 0) }, 0, 1);
            _tbHostPlayerName = new ModernTextBox
            {
                Text = Environment.UserName,
                Width = 220,
                Height = 32
            };
            table.Controls.Add(_tbHostPlayerName, 1, 1);

            // Row 2: Port
            table.Controls.Add(new Label { Text = "Game Port (UDP):", Font = Theme.FontRegularBold, ForeColor = Theme.TextSecondary, Padding = new Padding(0, 6, 0, 0) }, 0, 2);
            _nudHostPort = new NumericUpDown
            {
                Minimum = 1024,
                Maximum = 65535,
                Value = 27016,
                Width = 100,
                BackColor = Theme.BgInput,
                ForeColor = Theme.TextPrimary,
                Font = Theme.FontRegular
            };
            table.Controls.Add(_nudHostPort, 1, 2);

            // Row 3: Checkboxes
            table.Controls.Add(new Label { Text = "Options:", Font = Theme.FontRegularBold, ForeColor = Theme.TextSecondary, Padding = new Padding(0, 6, 0, 0) }, 0, 3);
            FlowLayoutPanel checkPanel = new FlowLayoutPanel { Dock = DockStyle.Fill };
            _cbHostBroadcast = new CheckBox
            {
                Text = "Broadcast lobby on LAN (UDP 27015)",
                Checked = true,
                AutoSize = true,
                ForeColor = Theme.TextPrimary,
                Font = Theme.FontRegular,
                Margin = new Padding(0, 6, 16, 0)
            };
            _cbHostSkipIntro = new CheckBox
            {
                Text = "Skip introductory cutscenes (-nogameintro)",
                Checked = false,
                AutoSize = true,
                ForeColor = Theme.TextSecondary,
                Font = Theme.FontRegular,
                Margin = new Padding(0, 6, 0, 0)
            };
            checkPanel.Controls.Add(_cbHostBroadcast);
            checkPanel.Controls.Add(_cbHostSkipIntro);
            table.Controls.Add(checkPanel, 1, 3);

            // Row 4: Start Button
            _btnStartHost = new ModernButton
            {
                Text = "🚀   START CO-OP HOST",
                Width = 240,
                Height = 44,
                Variant = ButtonVariant.PrimaryGreen,
                Font = Theme.FontHeroBtn
            };
            _btnStartHost.Click += (s, e) => StartHostingSession();
            table.Controls.Add(_btnStartHost, 1, 4);

            // Bottom Live Info Card
            ModernCard liveCard = new ModernCard
            {
                Dock = DockStyle.Bottom,
                Height = 130,
                Title = "Live Session Information",
                Subtitle = "Details to share with your co-op partner"
            };

            _lblHostLiveStatus = new Label
            {
                Text = "Host is currently idle. Click 'Start Co-op Host' above to launch Clear Sky.",
                Font = Theme.FontRegular,
                ForeColor = Theme.TextSecondary,
                Location = new Point(16, 50),
                AutoSize = true
            };

            _lblHostAddressCopy = new Label
            {
                Text = string.Format("Your LAN Join Address: {0}:27016 (Tell your partner to enter this in Direct IP)", _localIp),
                Font = Theme.FontRegularBold,
                ForeColor = Theme.AccentCyan,
                Location = new Point(16, 80),
                AutoSize = true,
                Cursor = Cursors.Hand
            };
            _lblHostAddressCopy.Click += (s, e) =>
            {
                Clipboard.SetText(string.Format("{0}:{1}", _localIp, (int)_nudHostPort.Value));
                MessageBox.Show("Server IP and port copied to clipboard!", "Address Copied", MessageBoxButtons.OK, MessageBoxIcon.Information);
            };

            liveCard.Controls.Add(_lblHostLiveStatus);
            liveCard.Controls.Add(_lblHostAddressCopy);

            configCard.Controls.Add(table);
            _pageHost.Controls.Add(configCard);
            _pageHost.Controls.Add(liveCard);
        }

        private void BuildSettingsPage()
        {
            _pageSettings = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest,
                Padding = new Padding(0, 10, 0, 0)
            };

            ModernCard pathCard = new ModernCard
            {
                Dock = DockStyle.Top,
                Height = 220,
                Title = "Game Installation & Co-op Mod Setup",
                Subtitle = "Locate S.T.A.L.K.E.R.: Clear Sky and deploy x64 mod files"
            };

            Label lblPath = new Label
            {
                Text = "Game Root Directory (contains fsgame.ltx):",
                Font = Theme.FontRegularBold,
                ForeColor = Theme.TextSecondary,
                Location = new Point(16, 50),
                AutoSize = true
            };

            _tbGamePath = new ModernTextBox
            {
                Location = new Point(16, 75),
                Width = 520,
                Height = 34
            };
            _tbGamePath.TextChanged += (s, e) => UpdateModStatus();

            _btnBrowse = new ModernButton
            {
                Text = "Browse...",
                Width = 90,
                Height = 34,
                Location = new Point(545, 75),
                Variant = ButtonVariant.Secondary
            };
            _btnBrowse.Click += (s, e) => BrowseGamePath();

            _btnRedetect = new ModernButton
            {
                Text = "🔄 Auto-Detect",
                Width = 120,
                Height = 34,
                Location = new Point(645, 75),
                Variant = ButtonVariant.Secondary
            };
            _btnRedetect.Click += (s, e) => ReDetectGame();

            _lblSettingsModStatus = new Label
            {
                Text = "Checking status...",
                Font = Theme.FontRegularBold,
                ForeColor = Theme.AccentAmber,
                Location = new Point(16, 122),
                AutoSize = true
            };

            _btnDeployMod = new ModernButton
            {
                Text = "🛠️   INSTALL / DEPLOY MOD FILES",
                Width = 260,
                Height = 38,
                Location = new Point(16, 155),
                Variant = ButtonVariant.PrimaryCyan
            };
            _btnDeployMod.Click += (s, e) => DeployModFiles();

            pathCard.Controls.Add(lblPath);
            pathCard.Controls.Add(_tbGamePath);
            pathCard.Controls.Add(_btnBrowse);
            pathCard.Controls.Add(_btnRedetect);
            pathCard.Controls.Add(_lblSettingsModStatus);
            pathCard.Controls.Add(_btnDeployMod);

            ModernCard prefCard = new ModernCard
            {
                Dock = DockStyle.Fill,
                Title = "Preferences & Shortcuts",
                Subtitle = "Convenience tools and game file access"
            };

            _cbGlobalSkipIntro = new CheckBox
            {
                Text = "Skip introductory storyline cutscenes & video (-nogameintro)",
                Checked = false,
                AutoSize = true,
                ForeColor = Theme.TextPrimary,
                Font = Theme.FontRegular,
                Location = new Point(16, 55)
            };

            _btnOpenGameFolder = new ModernButton
            {
                Text = "📂 Open Game Folder",
                Width = 170,
                Height = 34,
                Location = new Point(16, 95),
                Variant = ButtonVariant.Secondary
            };
            _btnOpenGameFolder.Click += (s, e) => OpenGameFolder();

            _btnOpenLogsFolder = new ModernButton
            {
                Text = "📂 Open Logs & Saves",
                Width = 170,
                Height = 34,
                Location = new Point(196, 95),
                Variant = ButtonVariant.Secondary
            };
            _btnOpenLogsFolder.Click += (s, e) => OpenLogsFolder();

            prefCard.Controls.Add(_cbGlobalSkipIntro);
            prefCard.Controls.Add(_btnOpenGameFolder);
            prefCard.Controls.Add(_btnOpenLogsFolder);

            _pageSettings.Controls.Add(prefCard);
            _pageSettings.Controls.Add(pathCard);
        }

        private void BuildDiagnosticsPage()
        {
            _pageDiag = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest,
                Padding = new Padding(0, 10, 0, 0)
            };

            ModernCard card = new ModernCard
            {
                Dock = DockStyle.Fill,
                Title = "One-Click Diagnostic Pack Generator",
                Subtitle = "Compresses logs, crash dumps, and saves into a single ZIP for instant bug reporting"
            };

            Label lblDesc = new Label
            {
                Text = "If you or your friend encounter any desync, crash, or unexpected behavior during gameplay,\n" +
                       "click the button below. A complete diagnostic ZIP containing latest logs, crash dumps (.mdmp),\n" +
                       "user settings, and save files will be generated.",
                Font = Theme.FontRegular,
                ForeColor = Theme.TextSecondary,
                Location = new Point(16, 50),
                AutoSize = true
            };

            _btnGenerateDiag = new ModernButton
            {
                Text = "📦   GENERATE DIAGNOSTIC ZIP",
                Width = 260,
                Height = 40,
                Location = new Point(16, 120),
                Variant = ButtonVariant.PrimaryCyan,
                Font = Theme.FontSubtitle
            };
            _btnGenerateDiag.Click += (s, e) => CollectDiagnostics();

            _btnOpenDiagFolder = new ModernButton
            {
                Text = "📂 Open diagnostics/ Folder",
                Width = 220,
                Height = 40,
                Location = new Point(286, 120),
                Variant = ButtonVariant.Secondary,
                Visible = false
            };
            _btnOpenDiagFolder.Click += (s, e) =>
            {
                string diagDir = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "diagnostics");
                if (Directory.Exists(diagDir)) Process.Start("explorer.exe", diagDir);
            };

            _lblDiagStatus = new Label
            {
                Text = "Ready to generate.",
                Font = Theme.FontRegularBold,
                ForeColor = Theme.TextSecondary,
                Location = new Point(16, 175),
                AutoSize = true
            };

            card.Controls.Add(lblDesc);
            card.Controls.Add(_btnGenerateDiag);
            card.Controls.Add(_btnOpenDiagFolder);
            card.Controls.Add(_lblDiagStatus);

            _pageDiag.Controls.Add(card);
        }

        private void InitializeLogic()
        {
            _localIp = NetworkHelper.GetLocalIpAddress();
            _lblSidebarIp.Text = "LAN: " + _localIp;

            // Auto-detect game
            _gamePath = GameLocator.DetectGamePath();
            if (!string.IsNullOrEmpty(_gamePath))
            {
                _tbGamePath.Text = _gamePath;
                UpdateModStatus();
            }
            else
            {
                _lblSettingsModStatus.Text = "Game not automatically detected. Please browse for game folder.";
                _lblSettingsModStatus.ForeColor = Theme.AccentAmber;
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
                _lblSettingsModStatus.Text = "No game folder selected.";
                _lblSettingsModStatus.ForeColor = Theme.AccentRed;
                _lblModBadge.Text = "No Game Path";
                _lblModBadge.ForeColor = Theme.AccentRed;
                _lblModBadge.BackColor = Color.FromArgb(45, 20, 20);
                return;
            }

            if (ModDeployer.IsModInstalled(norm))
            {
                _lblSettingsModStatus.Text = "✔ Mod files installed (OpenXRay x64 binaries present)";
                _lblSettingsModStatus.ForeColor = Theme.AccentGreen;
                _lblModBadge.Text = "✔ Game & Mod Ready";
                _lblModBadge.ForeColor = Theme.AccentGreen;
                _lblModBadge.BackColor = Color.FromArgb(20, 45, 35);
            }
            else
            {
                _lblSettingsModStatus.Text = "⚠ Mod binaries missing. Click 'Install / Deploy Mod Files' below.";
                _lblSettingsModStatus.ForeColor = Theme.AccentAmber;
                _lblModBadge.Text = "⚠ Mod Missing";
                _lblModBadge.ForeColor = Theme.AccentAmber;
                _lblModBadge.BackColor = Color.FromArgb(45, 38, 20);
            }
        }

        private void OnLobbyServerUpdated(LobbyServerInfo server)
        {
            if (this.InvokeRequired)
            {
                this.BeginInvoke(new Action<LobbyServerInfo>(OnLobbyServerUpdated), server);
                return;
            }

            _lblLobbyEmpty.Visible = false;

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

            if (_lvServers.Items.Count == 0)
            {
                _lblLobbyEmpty.Visible = true;
            }
        }

        private void RefreshLobbyServers()
        {
            _lvServers.Items.Clear();
            _lblLobbyEmpty.Visible = true;
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
            if (!int.TryParse(_tbDirectPort.Text.Trim(), out port))
            {
                MessageBox.Show("Please enter a valid numeric UDP port (e.g. 27016).", "Invalid Port", MessageBoxButtons.OK, MessageBoxIcon.Warning);
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
                SwitchPage(2);
                return;
            }

            string playerName = _tbPlayerName.Text.Trim();
            string error;
            bool skipIntro = _cbGlobalSkipIntro != null && _cbGlobalSkipIntro.Checked;
            Process proc = SessionLauncher.StartClient(gamePath, ip, port, playerName, skipIntro, out error);
            if (proc != null)
            {
                _lblNetworkBadge.Text = string.Format("● Connected to {0}:{1}", ip, port);
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
                SwitchPage(2);
                return;
            }

            int port = (int)_nudHostPort.Value;
            string serverName = _tbHostServerName.Text.Trim();
            string hostName = _tbHostPlayerName.Text.Trim();

            string error;
            bool skipIntro = (_cbHostSkipIntro != null && _cbHostSkipIntro.Checked) ||
                             (_cbGlobalSkipIntro != null && _cbGlobalSkipIntro.Checked);
            Process proc = SessionLauncher.StartHost(gamePath, port, skipIntro, out error);
            if (proc != null)
            {
                // Start advertising on LAN
                if (_cbHostBroadcast.Checked)
                {
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
                }

                _lblHostLiveStatus.Text = string.Format("🟢 HOST RUNNING on port {0}. Game launched successfully.", port);
                _lblHostLiveStatus.ForeColor = Theme.AccentGreen;
                _lblHostAddressCopy.Text = string.Format("Your LAN Join Address: {0}:{1} (Click to copy for friend)", _localIp, port);
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

        private void ReDetectGame()
        {
            string path = GameLocator.DetectGamePath();
            if (!string.IsNullOrEmpty(path))
            {
                _tbGamePath.Text = path;
                UpdateModStatus();
                MessageBox.Show("Game detected at:\n" + path, "Game Found", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            else
            {
                MessageBox.Show("Could not automatically locate Clear Sky. Please select the folder manually.", "Not Found", MessageBoxButtons.OK, MessageBoxIcon.Warning);
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

        private void OpenGameFolder()
        {
            string p = ModDeployer.NormalizeGamePath(_tbGamePath.Text);
            if (!string.IsNullOrEmpty(p) && Directory.Exists(p))
            {
                Process.Start("explorer.exe", p);
            }
            else
            {
                MessageBox.Show("Invalid or non-existent game directory.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        private void OpenLogsFolder()
        {
            string p = ModDeployer.NormalizeGamePath(_tbGamePath.Text);
            if (!string.IsNullOrEmpty(p) && Directory.Exists(p))
            {
                string logs = Path.Combine(p, @"_appdata_\logs");
                if (!Directory.Exists(logs)) Directory.CreateDirectory(logs);
                Process.Start("explorer.exe", logs);
            }
        }

        private void CollectDiagnostics()
        {
            try
            {
                _lblDiagStatus.Text = "Collecting diagnostics (logs, dumps, saves)...";
                _lblDiagStatus.ForeColor = Theme.AccentAmber;

                string scriptPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, @"scripts\collect_diagnostics.ps1");
                if (!File.Exists(scriptPath))
                {
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

                    _lblDiagStatus.Text = "✔ Diagnostic pack generated successfully in diagnostics/ folder!";
                    _lblDiagStatus.ForeColor = Theme.AccentGreen;
                    _btnOpenDiagFolder.Visible = true;
                }
                else
                {
                    _lblDiagStatus.Text = "Diagnostic script not found at " + scriptPath;
                    _lblDiagStatus.ForeColor = Theme.AccentRed;
                }
            }
            catch (Exception ex)
            {
                _lblDiagStatus.Text = "Error: " + ex.Message;
                _lblDiagStatus.ForeColor = Theme.AccentRed;
            }
        }

        private static void EnableDoubleBuffer(Control c)
        {
            try
            {
                typeof(Control).InvokeMember("DoubleBuffered",
                    BindingFlags.SetProperty | BindingFlags.Instance | BindingFlags.NonPublic,
                    null, c, new object[] { true });
            }
            catch { }
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            if (_beaconService != null)
            {
                _beaconService.Dispose();
            }
            base.OnFormClosing(e);
        }
    }
}
