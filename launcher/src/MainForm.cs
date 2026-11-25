using System;
using System.IO;
using System.IO.Compression;
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
        private StatusPillBadge _lblNetworkBadge;
        private StatusPillBadge _lblModBadge;

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
        private ModernButton _btnQuickRecent;
        private Label _lblLobbyEmpty;
        private TacticalRadarPanel _radarPanel;

        // Page 2: Host Controls
        private ModernTextBox _tbHostServerName;
        private ModernTextBox _tbHostPlayerName;
        private TacticalPortStepper _nudHostPort;
        private TacticalToggle _cbHostBroadcast;
        private TacticalToggle _cbHostSkipIntro;
        private ModernButton _btnStartHost;
        private ModernButton _btnCopyHostIp;
        private Label _lblHostLiveStatus;
        private Label _lblHostAddressCopy;

        // Page 3: Settings Controls
        private ModernTextBox _tbGamePath;
        private ModernButton _btnBrowse;
        private ModernButton _btnDeployMod;
        private Label _lblSettingsModStatus;
        private TacticalToggle _cbGlobalSkipIntro;
        private ModernButton _btnOpenGameFolder;
        private ModernButton _btnOpenLogsFolder;
        private ModernButton _btnFirewall;
        private ModernButton _btnRedetect;

        // Page 4: Diagnostics Controls
        private ModernButton _btnGenerateDiag;
        private ModernButton _btnOpenDiagFolder;
        private Label _lblDiagStatus;
        private Label _lblDiagTelemetry;
        private LauncherConfig _config;
        private Timer _saveDebounceTimer;

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
            this.Text = "S.T.A.L.K.E.R. Clear Sky - Tactical PDA Co-op";
            this.FormBorderStyle = FormBorderStyle.None;
            this.Size = new Size(980, 620);
            this.MinimumSize = new Size(920, 560);
            this.StartPosition = FormStartPosition.CenterScreen;
            this.BackColor = Theme.BgDarkest;
            this.ForeColor = Theme.TextPrimary;
            this.Font = Theme.FontRegular;
            this.KeyPreview = true;
            this.KeyDown += (s, e) =>
            {
                if (e.KeyCode == Keys.F5)
                {
                    RefreshLobbyServers();
                }
            };
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            using (Pen borderPen = new Pen(Theme.BorderLight, 1f))
            {
                e.Graphics.DrawRectangle(borderPen, 0, 0, Width - 1, Height - 1);
            }
        }

        private void InitializeComponents()
        {
            _titleBar = new TitleBarControl(this)
            {
                Dock = DockStyle.Top,
                Height = 38
            };

            Panel bodyPanel = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest
            };

            BuildSidebar();

            _contentPanel = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest,
                Padding = new Padding(18, 12, 18, 14)
            };

            BuildTopBar();

            Panel pagesContainer = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest
            };

            BuildLobbyPage();
            BuildHostPage();
            BuildSettingsPage();
            BuildDiagnosticsPage();

            _pages = new Panel[] { _pageLobby, _pageHost, _pageSettings, _pageDiag };
            _navButtons = new ModernButton[] { _btnNavLobby, _btnNavHost, _btnNavSettings, _btnNavDiag };

            foreach (Panel p in _pages)
            {
                pagesContainer.Controls.Add(p);
            }

            _contentPanel.Controls.Add(pagesContainer);
            _contentPanel.Controls.Add(_topBar);
            _topBar.SendToBack();
            pagesContainer.BringToFront();

            bodyPanel.Controls.Add(_contentPanel);
            bodyPanel.Controls.Add(_sidebar);
            _sidebar.BringToFront();
            _contentPanel.BringToFront();

            this.Controls.Add(bodyPanel);
            this.Controls.Add(_titleBar);
            _titleBar.BringToFront();
            bodyPanel.BringToFront();

            SwitchPage(0);
        }

        private void BuildSidebar()
        {
            _sidebar = new Panel
            {
                Dock = DockStyle.Left,
                Width = 224,
                BackColor = Theme.BgSidebar
            };
            _sidebar.Paint += (s, e) =>
            {
                using (Pen p = new Pen(Theme.Border, 1f))
                {
                    e.Graphics.DrawLine(p, _sidebar.Width - 1, 0, _sidebar.Width - 1, _sidebar.Height);
                }
            };

            Panel headerPanel = new Panel
            {
                Dock = DockStyle.Top,
                Height = 76,
                BackColor = Theme.BgSidebar,
                Padding = new Padding(18, 14, 16, 10)
            };

            Label lblFaction = new Label
            {
                Text = "CLEAR SKY",
                Font = Theme.FontAppTitle,
                ForeColor = Theme.TextPrimary,
                BackColor = Theme.BgSidebar,
                UseMnemonic = false,
                Dock = DockStyle.Top,
                Height = 25
            };
            Label lblSub = new Label
            {
                Text = "TACTICAL PDA // CO-OP",
                Font = Theme.FontMonoSmall,
                ForeColor = Theme.AccentCyan,
                BackColor = Theme.BgSidebar,
                UseMnemonic = false,
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

            Panel bottomProfile = new Panel
            {
                Dock = DockStyle.Bottom,
                Height = 76,
                BackColor = Color.FromArgb(12, 16, 24),
                Padding = new Padding(16, 10, 14, 10)
            };
            bottomProfile.Paint += (s, e) =>
            {
                using (Pen p = new Pen(Theme.Border, 1f))
                    e.Graphics.DrawLine(p, 0, 0, bottomProfile.Width - 1, 0);
            };

            _lblSidebarPlayer = new Label
            {
                Text = "Stalker: " + Environment.UserName,
                Font = Theme.FontRegularBold,
                ForeColor = Theme.TextPrimary,
                BackColor = Color.FromArgb(12, 16, 24),
                UseMnemonic = false,
                Padding = new Padding(14, 0, 0, 0),
                Dock = DockStyle.Top,
                Height = 22
            };
            _lblSidebarPlayer.Paint += (s, e) =>
            {
                e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
                using (SolidBrush halo = new SolidBrush(Color.FromArgb(70, Theme.AccentGreen)))
                using (SolidBrush dot = new SolidBrush(Theme.AccentGreen))
                {
                    e.Graphics.FillEllipse(halo, 0, 5, 11, 11);
                    e.Graphics.FillEllipse(dot, 2, 7, 7, 7);
                }
            };
            _lblSidebarIp = new Label
            {
                Text = "LAN: " + _localIp,
                Font = Theme.FontMonoSmall,
                ForeColor = Theme.AccentCyan,
                BackColor = Color.FromArgb(12, 16, 24),
                UseMnemonic = false,
                Cursor = Cursors.Hand,
                Dock = DockStyle.Top,
                Height = 18
            };
            _lblSidebarIp.Click += (s, e) =>
            {
                try
                {
                    Clipboard.SetText(_localIp);
                    _lblSidebarIp.Text = "COPIED: " + _localIp;
                    Timer t = new Timer { Interval = 1000 };
                    t.Tick += (ts, te) =>
                    {
                        t.Stop();
                        t.Dispose();
                        if (_lblSidebarIp != null && !_lblSidebarIp.IsDisposed)
                            _lblSidebarIp.Text = "LAN: " + _localIp;
                    };
                    t.Start();
                }
                catch { }
            };

            Label lblCopyHint = new Label
            {
                Text = "Click IP to copy",
                Font = new Font("Segoe UI", 7.25f, FontStyle.Regular),
                ForeColor = Theme.TextMuted,
                BackColor = Color.FromArgb(12, 16, 24),
                UseMnemonic = false,
                Dock = DockStyle.Top,
                Height = 14
            };

            bottomProfile.Controls.Add(lblCopyHint);
            bottomProfile.Controls.Add(_lblSidebarIp);
            bottomProfile.Controls.Add(_lblSidebarPlayer);

            Panel navContainer = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgSidebar,
                Padding = new Padding(10, 12, 10, 10)
            };

            _btnNavLobby = CreateNavButton("Lobby & Radar", IconType.Radar, "01", 0);
            _btnNavHost = CreateNavButton("Host Campaign", IconType.Signal, "02", 1);
            _btnNavSettings = CreateNavButton("Mod & Config", IconType.Gear, "03", 2);
            _btnNavDiag = CreateNavButton("Diagnostics", IconType.Wrench, "04", 3);

            _btnNavLobby.Top = 14;
            _btnNavHost.Top = 62;
            _btnNavSettings.Top = 110;
            _btnNavDiag.Top = 158;

            navContainer.Controls.Add(_btnNavLobby);
            navContainer.Controls.Add(_btnNavHost);
            navContainer.Controls.Add(_btnNavSettings);
            navContainer.Controls.Add(_btnNavDiag);

            _sidebar.Controls.Add(navContainer);
            _sidebar.Controls.Add(bottomProfile);
            _sidebar.Controls.Add(headerPanel);

            headerPanel.SendToBack();
            bottomProfile.SendToBack();
            navContainer.BringToFront();
        }

        private ModernButton CreateNavButton(string text, IconType icon, string indexBadge, int pageIndex)
        {
            ModernButton btn = new ModernButton
            {
                Text = text,
                Icon = icon,
                BadgeText = indexBadge,
                Variant = ButtonVariant.Nav,
                Left = 10,
                Width = 202,
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
                Height = 40,
                BackColor = Theme.BgDarkest
            };

            _lblPageTitle = new Label
            {
                Text = "LOBBY & SIGNAL RADAR",
                Font = Theme.FontTitle,
                ForeColor = Theme.TextPrimary,
                BackColor = Theme.BgDarkest,
                AutoSize = true,
                UseMnemonic = false,
                Location = new Point(0, 7)
            };

            FlowLayoutPanel badgePanel = new FlowLayoutPanel
            {
                Dock = DockStyle.Right,
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                WrapContents = false,
                BackColor = Theme.BgDarkest,
                FlowDirection = FlowDirection.LeftToRight
            };

            _lblNetworkBadge = new StatusPillBadge
            {
                Text = "LAN BEACON: 27015",
                AccentColor = Theme.AccentGreen,
                FillColor = Color.FromArgb(14, 40, 32),
                Margin = new Padding(0, 5, 8, 0)
            };

            _lblModBadge = new StatusPillBadge
            {
                Text = "CHECKING MOD...",
                AccentColor = Theme.AccentAmber,
                FillColor = Color.FromArgb(42, 34, 16),
                Margin = new Padding(0, 5, 0, 0)
            };

            badgePanel.Controls.Add(_lblNetworkBadge);
            badgePanel.Controls.Add(_lblModBadge);

            _topBar.Controls.Add(_lblPageTitle);
            _topBar.Controls.Add(badgePanel);
            _contentPanel.Controls.Add(_topBar);
        }

        private void SwitchPage(int index)
        {
            SaveConfig();

            for (int i = 0; i < _pages.Length; i++)
            {
                bool active = (i == index);
                _pages[i].Visible = active;
                _navButtons[i].IsActiveNav = active;
            }

            switch (index)
            {
                case 0: _lblPageTitle.Text = "LOBBY & SIGNAL RADAR"; break;
                case 1: _lblPageTitle.Text = "HOST CO-OP CAMPAIGN"; break;
                case 2: _lblPageTitle.Text = "MOD & SYSTEM CONFIG"; break;
                case 3: _lblPageTitle.Text = "DIAGNOSTICS & TELEMETRY"; break;
            }

            if (index == 3) UpdateDiagnosticsTelemetry();
        }

        private void BuildLobbyPage()
        {
            _pageLobby = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest,
                Padding = new Padding(0, 8, 0, 0)
            };

            // Upper split container: Left = Server List Card, Right = Tactical Radar Card
            Panel upperRow = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest
            };

            ModernCard radarCard = new ModernCard
            {
                Dock = DockStyle.Right,
                Width = 216,
                Title = "PDA Signal Radar",
                Subtitle = "LAN Beacon Scanner",
                TopAccentColor = Theme.AccentCyan
            };

            _radarPanel = new TacticalRadarPanel
            {
                Dock = DockStyle.Fill
            };
            radarCard.Controls.Add(_radarPanel);

            Panel colSpacer = new Panel
            {
                Dock = DockStyle.Right,
                Width = 10,
                BackColor = Theme.BgDarkest
            };

            ModernCard serverCard = new ModernCard
            {
                Dock = DockStyle.Fill,
                Title = "Available LAN Co-op Sessions",
                Subtitle = "Active squads broadcasting on UDP port 27015",
                HeaderTag = "LAN",
                TopAccentColor = Theme.AccentCyan
            };

            _lvServers = new ListView
            {
                View = View.Details,
                FullRowSelect = true,
                GridLines = false,
                BorderStyle = BorderStyle.None,
                BackColor = Theme.BgInput,
                ForeColor = Theme.TextPrimary,
                Font = Theme.FontRegular,
                Dock = DockStyle.Fill,
                OwnerDraw = true,
                Visible = false
            };
            EnableDoubleBuffer(_lvServers);

            _lvServers.DrawColumnHeader += (s, e) =>
            {
                using (SolidBrush bgBrush = new SolidBrush(Theme.BgSidebar))
                {
                    e.Graphics.FillRectangle(bgBrush, e.Bounds);
                }
                using (Pen borderPen = new Pen(Theme.Border, 1f))
                {
                    e.Graphics.DrawLine(borderPen, e.Bounds.Left, e.Bounds.Bottom - 1, e.Bounds.Right, e.Bounds.Bottom - 1);
                    e.Graphics.DrawLine(borderPen, e.Bounds.Right - 1, e.Bounds.Top + 4, e.Bounds.Right - 1, e.Bounds.Bottom - 5);
                }
                using (SolidBrush textBrush = new SolidBrush(Theme.TextSecondary))
                {
                    StringFormat sf = new StringFormat { Alignment = StringAlignment.Near, LineAlignment = StringAlignment.Center };
                    Rectangle r = new Rectangle(e.Bounds.Left + 8, e.Bounds.Top, e.Bounds.Width - 12, e.Bounds.Height);
                    e.Graphics.DrawString(e.Header.Text, Theme.FontSmallBold, textBrush, r, sf);
                }
            };

            _lvServers.DrawItem += (s, e) => { e.DrawDefault = false; };

            _lvServers.DrawSubItem += (s, e) =>
            {
                bool isSelected = e.Item.Selected;
                Color bg = isSelected
                    ? Color.FromArgb(24, 46, 72)
                    : ((e.ItemIndex % 2 == 1) ? Color.FromArgb(16, 22, 32) : Theme.BgInput);
                using (SolidBrush brush = new SolidBrush(bg))
                {
                    e.Graphics.FillRectangle(brush, e.Bounds);
                }
                Color textCol = isSelected ? Color.White : ((e.ColumnIndex == 0) ? Theme.AccentCyan : Theme.TextPrimary);
                Font cellFont = (e.ColumnIndex == 4) ? Theme.FontMonoSmall : Theme.FontRegular;
                using (SolidBrush textBrush = new SolidBrush(textCol))
                {
                    StringFormat sf = new StringFormat { LineAlignment = StringAlignment.Center };
                    Rectangle r = new Rectangle(e.Bounds.Left + 8, e.Bounds.Top, e.Bounds.Width - 12, e.Bounds.Height);
                    e.Graphics.DrawString(e.SubItem.Text, cellFont, textBrush, r, sf);
                }
                if (isSelected && e.ColumnIndex == 0)
                {
                    using (SolidBrush selBar = new SolidBrush(Theme.AccentCyan))
                    {
                        e.Graphics.FillRectangle(selBar, e.Bounds.Left, e.Bounds.Top, 3, e.Bounds.Height);
                    }
                }
            };

            _lvServers.Columns.Add("Server Name", 160);
            _lvServers.Columns.Add("Host", 95);
            _lvServers.Columns.Add("Zone", 80);
            _lvServers.Columns.Add("Squad", 55);
            _lvServers.Columns.Add("IP:Port", 120);
            _lvServers.DoubleClick += (s, e) => JoinSelectedServer();
            _lvServers.KeyDown += (s, e) =>
            {
                if (e.KeyCode == Keys.Enter && _lvServers.SelectedItems.Count > 0)
                {
                    e.SuppressKeyPress = true;
                    JoinSelectedServer();
                }
            };

            _lblLobbyEmpty = new Label
            {
                Text = "Listening for Clear Sky co-op beacons on your LAN...\n\nHost a campaign on PC 1, or enter the host IP below.",
                Font = Theme.FontRegular,
                ForeColor = Theme.TextMuted,
                BackColor = Theme.BgCard,
                TextAlign = ContentAlignment.MiddleCenter,
                Dock = DockStyle.Fill,
                UseMnemonic = false
            };

            Panel listContainer = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgCard,
                Padding = new Padding(0, 0, 0, 8)
            };
            listContainer.Controls.Add(_lblLobbyEmpty);
            listContainer.Controls.Add(_lvServers);

            Panel listActionRow = new Panel
            {
                Dock = DockStyle.Bottom,
                Height = 42,
                BackColor = Theme.BgCard,
                Padding = new Padding(0, 6, 0, 0)
            };

            Label lblNick = new Label
            {
                Text = "Callsign:",
                Font = Theme.FontRegularBold,
                ForeColor = Theme.TextSecondary,
                BackColor = Theme.BgCard,
                UseMnemonic = false,
                AutoSize = true,
                Location = new Point(0, 12)
            };

            _tbPlayerName = new ModernTextBox
            {
                Text = Environment.UserName,
                Width = 128,
                Height = 34,
                Location = new Point(66, 6)
            };
            _tbPlayerName.TextChanged += (s, e) =>
            {
                if (_lblSidebarPlayer != null) _lblSidebarPlayer.Text = "Stalker: " + _tbPlayerName.Text;
                ScheduleAutoSave();
            };
            _tbPlayerName.Leave += (s, e) => SaveConfig();

            _btnRefreshLobby = new ModernButton
            {
                Text = "Scan",
                Icon = IconType.Refresh,
                Width = 90,
                Height = 34,
                Location = new Point(204, 6),
                Variant = ButtonVariant.Secondary
            };
            _btnRefreshLobby.Click += (s, e) => RefreshLobbyServers();

            _btnJoinSelected = new ModernButton
            {
                Text = "JOIN SESSION",
                Icon = IconType.Play,
                Width = 158,
                Height = 34,
                Dock = DockStyle.Right,
                Variant = ButtonVariant.PrimaryCyan,
                Font = Theme.FontSubtitle
            };
            _btnJoinSelected.Click += (s, e) => JoinSelectedServer();

            listActionRow.Controls.Add(_btnJoinSelected);
            listActionRow.Controls.Add(_btnRefreshLobby);
            listActionRow.Controls.Add(_tbPlayerName);
            listActionRow.Controls.Add(lblNick);

            serverCard.Controls.Add(listContainer);
            serverCard.Controls.Add(listActionRow);

            upperRow.Controls.Add(serverCard);
            upperRow.Controls.Add(colSpacer);
            upperRow.Controls.Add(radarCard);
            radarCard.SendToBack();
            colSpacer.SendToBack();
            serverCard.BringToFront();

            Panel rowSpacer = new Panel
            {
                Dock = DockStyle.Bottom,
                Height = 10,
                BackColor = Theme.BgDarkest
            };

            ModernCard directCard = new ModernCard
            {
                Dock = DockStyle.Bottom,
                Height = 114,
                Title = "Direct IP Uplink",
                Subtitle = "Connect directly to a host over LAN, Radmin/ZeroTier, or Internet",
                HeaderTag = "DIRECT",
                TopAccentColor = Theme.BorderLight
            };

            FlowLayoutPanel directInputs = new FlowLayoutPanel
            {
                Dock = DockStyle.Bottom,
                Height = 40,
                BackColor = Theme.BgCard,
                WrapContents = false,
                Padding = new Padding(0, 2, 0, 0)
            };

            Label lblIp = new Label
            {
                Text = "IP:",
                Font = Theme.FontRegularBold,
                ForeColor = Theme.TextSecondary,
                BackColor = Theme.BgCard,
                AutoSize = true,
                UseMnemonic = false,
                Margin = new Padding(0, 8, 6, 0)
            };
            _tbDirectIp = new ModernTextBox { Text = "127.0.0.1", Width = 152, Height = 32, UseMonospace = true };

            Label lblPort = new Label
            {
                Text = "Port:",
                Font = Theme.FontRegularBold,
                ForeColor = Theme.TextSecondary,
                BackColor = Theme.BgCard,
                AutoSize = true,
                UseMnemonic = false,
                Margin = new Padding(12, 8, 6, 0)
            };
            _tbDirectPort = new ModernTextBox { Text = "27016", Width = 72, Height = 32, UseMonospace = true };

            KeyEventHandler directEnterHandler = (s, e) =>
            {
                if (e.KeyCode == Keys.Enter)
                {
                    e.SuppressKeyPress = true;
                    JoinDirectServer();
                }
            };
            _tbDirectIp.KeyDown += directEnterHandler;
            _tbDirectPort.KeyDown += directEnterHandler;
            _tbDirectIp.TextChanged += (s, e) => ScheduleAutoSave();
            _tbDirectIp.Leave += (s, e) => SaveConfig();
            _tbDirectPort.TextChanged += (s, e) => ScheduleAutoSave();
            _tbDirectPort.Leave += (s, e) => SaveConfig();

            _btnDirectConnect = new ModernButton
            {
                Text = "Connect Direct",
                Icon = IconType.Bolt,
                Width = 148,
                Height = 32,
                Margin = new Padding(14, 0, 0, 0),
                Variant = ButtonVariant.PrimaryCyan
            };
            _btnDirectConnect.Click += (s, e) => JoinDirectServer();

            _btnQuickRecent = new ModernButton
            {
                Text = "Last",
                Width = 175,
                Height = 32,
                Margin = new Padding(10, 0, 0, 0),
                Variant = ButtonVariant.Secondary,
                Font = Theme.FontMonoSmall,
                Visible = false
            };

            directInputs.Controls.Add(lblIp);
            directInputs.Controls.Add(_tbDirectIp);
            directInputs.Controls.Add(lblPort);
            directInputs.Controls.Add(_tbDirectPort);
            directInputs.Controls.Add(_btnDirectConnect);
            directInputs.Controls.Add(_btnQuickRecent);

            directCard.Controls.Add(directInputs);

            _pageLobby.Controls.Add(upperRow);
            _pageLobby.Controls.Add(rowSpacer);
            _pageLobby.Controls.Add(directCard);
            directCard.SendToBack();
            rowSpacer.SendToBack();
            upperRow.BringToFront();
        }

        private void BuildHostPage()
        {
            _pageHost = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest,
                Padding = new Padding(0, 8, 0, 0)
            };

            ModernCard configCard = new ModernCard
            {
                Dock = DockStyle.Fill,
                Title = "Squad Host Configuration",
                Subtitle = "Launch authoritative campaign server and advertise beacon",
                HeaderTag = "HOST",
                TopAccentColor = Theme.AccentGreen
            };

            TableLayoutPanel table = new TableLayoutPanel
            {
                Dock = DockStyle.Top,
                Height = 264,
                RowCount = 6,
                ColumnCount = 2,
                BackColor = Theme.BgCard,
                Padding = new Padding(0, 4, 0, 0)
            };
            table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 145));
            table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100f));

            table.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
            table.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
            table.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
            table.RowStyles.Add(new RowStyle(SizeType.Absolute, 34));
            table.RowStyles.Add(new RowStyle(SizeType.Absolute, 38));
            table.RowStyles.Add(new RowStyle(SizeType.Absolute, 52));

            table.Controls.Add(CreateFieldLabel("Session Name:"), 0, 0);
            _tbHostServerName = new ModernTextBox
            {
                Text = Environment.UserName + "'s Clear Sky Campaign",
                Width = 360,
                Height = 32
            };
            _tbHostServerName.TextChanged += (s, e) => ScheduleAutoSave();
            _tbHostServerName.Leave += (s, e) => SaveConfig();
            table.Controls.Add(_tbHostServerName, 1, 0);

            table.Controls.Add(CreateFieldLabel("Host Callsign:"), 0, 1);
            _tbHostPlayerName = new ModernTextBox
            {
                Text = Environment.UserName,
                Width = 220,
                Height = 32
            };
            _tbHostPlayerName.TextChanged += (s, e) => ScheduleAutoSave();
            _tbHostPlayerName.Leave += (s, e) => SaveConfig();
            table.Controls.Add(_tbHostPlayerName, 1, 1);

            table.Controls.Add(CreateFieldLabel("Game Port (UDP):"), 0, 2);
            _nudHostPort = new TacticalPortStepper
            {
                Minimum = 1024,
                Maximum = 65535,
                Value = 27016
            };
            _nudHostPort.ValueChanged += (s, e) =>
            {
                SaveConfig();
                if (_lblHostAddressCopy != null)
                {
                    _lblHostAddressCopy.Text = string.Format("Uplink Address: {0}:{1}", _localIp, _nudHostPort.Value);
                }
            };
            table.Controls.Add(_nudHostPort, 1, 2);

            table.Controls.Add(CreateFieldLabel("LAN Broadcast:"), 0, 3);
            _cbHostBroadcast = new TacticalToggle
            {
                Text = "Advertise squad beacon on LAN (UDP 27015)",
                Checked = true,
                Width = 360,
                AccentColor = Theme.AccentGreen
            };
            _cbHostBroadcast.CheckedChanged += (s, e) => SaveConfig();
            table.Controls.Add(_cbHostBroadcast, 1, 3);

            table.Controls.Add(CreateFieldLabel("Intro Sequence:"), 0, 4);
            _cbHostSkipIntro = new TacticalToggle
            {
                Text = "Skip opening cutscene on start (-nogameintro)",
                Checked = false,
                Width = 360,
                AccentColor = Theme.AccentCyan
            };
            _cbHostSkipIntro.CheckedChanged += (s, e) => SaveConfig();
            table.Controls.Add(_cbHostSkipIntro, 1, 4);

            FlowLayoutPanel hostBtnRow = new FlowLayoutPanel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgCard,
                Margin = new Padding(0, 4, 0, 0)
            };

            _btnStartHost = new ModernButton
            {
                Text = "START CO-OP LOBBY",
                Icon = IconType.Play,
                Width = 230,
                Height = 42,
                Variant = ButtonVariant.PrimaryGreen,
                Font = Theme.FontHeroBtn
            };
            _btnStartHost.Click += (s, e) => StartHostingSession();

            ModernButton btnHostFirewall = new ModernButton
            {
                Text = "Firewall Setup",
                Icon = IconType.Shield,
                Width = 165,
                Height = 42,
                Margin = new Padding(12, 0, 0, 0),
                Variant = ButtonVariant.Secondary,
                Font = Theme.FontSubtitle
            };
            btnHostFirewall.Click += (s, e) => ConfigureFirewall();

            hostBtnRow.Controls.Add(_btnStartHost);
            hostBtnRow.Controls.Add(btnHostFirewall);
            table.Controls.Add(hostBtnRow, 1, 5);

            Panel spacer = new Panel
            {
                Dock = DockStyle.Bottom,
                Height = 10,
                BackColor = Theme.BgDarkest
            };

            ModernCard liveCard = new ModernCard
            {
                Dock = DockStyle.Bottom,
                Height = 144,
                Title = "Live Telemetry & Squad Invite",
                Subtitle = "Share this uplink address with your co-op partner",
                HeaderTag = "UPLINK",
                TopAccentColor = Theme.AccentCyan
            };

            _lblHostLiveStatus = new Label
            {
                Text = "Standby. Press 'START CO-OP LOBBY' to boot Clear Sky into the in-game lobby.",
                Font = Theme.FontRegular,
                ForeColor = Theme.TextSecondary,
                BackColor = Theme.BgCard,
                UseMnemonic = false,
                Location = new Point(16, 72),
                AutoSize = true
            };

            _lblHostAddressCopy = new Label
            {
                Text = string.Format("Uplink Address: {0}:27016", _localIp),
                Font = Theme.FontMonoBold,
                ForeColor = Theme.AccentCyan,
                BackColor = Theme.BgCard,
                UseMnemonic = false,
                Location = new Point(16, 102),
                AutoSize = true,
                Cursor = Cursors.Hand
            };
            _lblHostAddressCopy.Click += (s, e) => CopyHostAddress();

            _btnCopyHostIp = new ModernButton
            {
                Text = "Copy IP",
                Icon = IconType.Copy,
                Width = 112,
                Height = 32,
                Location = new Point(588, 96),
                Variant = ButtonVariant.Secondary
            };
            _btnCopyHostIp.Click += (s, e) => CopyHostAddress();

            liveCard.Controls.Add(_lblHostLiveStatus);
            liveCard.Controls.Add(_lblHostAddressCopy);
            liveCard.Controls.Add(_btnCopyHostIp);

            configCard.Controls.Add(table);
            _pageHost.Controls.Add(configCard);
            _pageHost.Controls.Add(spacer);
            _pageHost.Controls.Add(liveCard);
            liveCard.SendToBack();
            spacer.SendToBack();
            configCard.BringToFront();
        }

        private void CopyHostAddress()
        {
            try
            {
                string addr = string.Format("{0}:{1}", _localIp, _nudHostPort.Value);
                Clipboard.SetText(addr);
                _btnCopyHostIp.Text = "Copied!";
                Timer t = new Timer { Interval = 1000 };
                t.Tick += (s, e) =>
                {
                    t.Stop();
                    t.Dispose();
                    if (_btnCopyHostIp != null && !_btnCopyHostIp.IsDisposed)
                        _btnCopyHostIp.Text = "Copy IP";
                };
                t.Start();
            }
            catch { }
        }

        private Label CreateFieldLabel(string text)
        {
            return new Label
            {
                Text = text,
                Font = Theme.FontRegularBold,
                ForeColor = Theme.TextSecondary,
                BackColor = Theme.BgCard,
                AutoSize = true,
                UseMnemonic = false,
                Padding = new Padding(0, 7, 0, 0)
            };
        }

        private void BuildSettingsPage()
        {
            _pageSettings = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest,
                Padding = new Padding(0, 8, 0, 0)
            };

            ModernCard pathCard = new ModernCard
            {
                Dock = DockStyle.Top,
                Height = 232,
                Title = "Game Installation & OpenXRay x64 Deployment",
                Subtitle = "Locate S.T.A.L.K.E.R.: Clear Sky 1.5.10 (fsgame.ltx) and deploy co-op binaries",
                HeaderTag = "DEPLOY",
                TopAccentColor = Theme.AccentCyan
            };

            Label lblPath = new Label
            {
                Text = "Game Root Directory (contains fsgame.ltx):",
                Font = Theme.FontRegularBold,
                ForeColor = Theme.TextSecondary,
                BackColor = Theme.BgCard,
                UseMnemonic = false,
                Location = new Point(16, 70),
                AutoSize = true
            };

            _tbGamePath = new ModernTextBox
            {
                Location = new Point(16, 94),
                Width = 438,
                Height = 34,
                UseMonospace = true
            };
            _tbGamePath.TextChanged += (s, e) =>
            {
                UpdateModStatus();
                ScheduleAutoSave();
            };
            _tbGamePath.Leave += (s, e) => SaveConfig();

            _btnBrowse = new ModernButton
            {
                Text = "Browse...",
                Width = 96,
                Height = 34,
                Location = new Point(464, 94),
                Variant = ButtonVariant.Secondary
            };
            _btnBrowse.Click += (s, e) => BrowseGamePath();

            _btnRedetect = new ModernButton
            {
                Text = "Auto-Detect",
                Icon = IconType.Refresh,
                Width = 132,
                Height = 34,
                Location = new Point(570, 94),
                Variant = ButtonVariant.Secondary
            };
            _btnRedetect.Click += (s, e) => ReDetectGame();

            _lblSettingsModStatus = new Label
            {
                Text = "Checking status...",
                Font = Theme.FontRegularBold,
                ForeColor = Theme.AccentAmber,
                BackColor = Theme.BgCard,
                UseMnemonic = false,
                Location = new Point(16, 140),
                AutoSize = true
            };

            _btnDeployMod = new ModernButton
            {
                Text = "DEPLOY MOD FILES",
                Icon = IconType.Gear,
                Width = 250,
                Height = 40,
                Location = new Point(16, 172),
                Variant = ButtonVariant.PrimaryCyan
            };
            _btnDeployMod.Click += (s, e) => DeployModFiles();

            pathCard.Controls.Add(lblPath);
            pathCard.Controls.Add(_tbGamePath);
            pathCard.Controls.Add(_btnBrowse);
            pathCard.Controls.Add(_btnRedetect);
            pathCard.Controls.Add(_lblSettingsModStatus);
            pathCard.Controls.Add(_btnDeployMod);

            Panel spacer = new Panel
            {
                Dock = DockStyle.Top,
                Height = 10,
                BackColor = Theme.BgDarkest
            };

            ModernCard prefCard = new ModernCard
            {
                Dock = DockStyle.Fill,
                Title = "PDA Preferences & Quick Access",
                Subtitle = "Launch behavior, local storage shortcuts, and Windows Firewall",
                HeaderTag = "TOOLS",
                TopAccentColor = Theme.BorderLight
            };

            _cbGlobalSkipIntro = new TacticalToggle
            {
                Text = "Skip introductory storyline cutscenes & videos (-nogameintro)",
                Checked = false,
                Width = 520,
                Location = new Point(16, 74),
                AccentColor = Theme.AccentCyan
            };
            _cbGlobalSkipIntro.CheckedChanged += (s, e) => SaveConfig();

            _btnOpenGameFolder = new ModernButton
            {
                Text = "Open Game Folder",
                Icon = IconType.Folder,
                Width = 180,
                Height = 36,
                Location = new Point(16, 118),
                Variant = ButtonVariant.Secondary
            };
            _btnOpenGameFolder.Click += (s, e) => OpenGameFolder();

            _btnOpenLogsFolder = new ModernButton
            {
                Text = "Open Logs & Saves",
                Icon = IconType.Folder,
                Width = 180,
                Height = 36,
                Location = new Point(208, 118),
                Variant = ButtonVariant.Secondary
            };
            _btnOpenLogsFolder.Click += (s, e) => OpenLogsFolder();

            _btnFirewall = new ModernButton
            {
                Text = "Firewall Setup (1-Click)",
                Icon = IconType.Shield,
                Width = 210,
                Height = 36,
                Location = new Point(400, 118),
                Variant = ButtonVariant.Secondary
            };
            _btnFirewall.Click += (s, e) => ConfigureFirewall();

            prefCard.Controls.Add(_cbGlobalSkipIntro);
            prefCard.Controls.Add(_btnOpenGameFolder);
            prefCard.Controls.Add(_btnOpenLogsFolder);
            prefCard.Controls.Add(_btnFirewall);

            _pageSettings.Controls.Add(prefCard);
            _pageSettings.Controls.Add(spacer);
            _pageSettings.Controls.Add(pathCard);
            pathCard.SendToBack();
            spacer.SendToBack();
            prefCard.BringToFront();
        }

        private void BuildDiagnosticsPage()
        {
            _pageDiag = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Theme.BgDarkest,
                Padding = new Padding(0, 8, 0, 0)
            };

            ModernCard card = new ModernCard
            {
                Dock = DockStyle.Fill,
                Title = "Diagnostic Pack & Field Telemetry",
                Subtitle = "Bundles OpenXRay logs, crash dumps (.mdmp), user.ltx and saves into a single ZIP",
                HeaderTag = "DIAG",
                TopAccentColor = Theme.AccentAmber
            };

            Label lblDesc = new Label
            {
                Text = "If you or your squadmate encounter a desync, crash, or quest lock during co-op gameplay,\n" +
                       "generate a diagnostic pack below. It archives recent logs, minidumps, and saves.",
                Font = Theme.FontRegular,
                ForeColor = Theme.TextSecondary,
                BackColor = Theme.BgCard,
                UseMnemonic = false,
                Location = new Point(16, 72),
                AutoSize = true
            };

            Panel telemetryBox = new Panel
            {
                Location = new Point(16, 124),
                Size = new Size(686, 132),
                BackColor = Theme.BgInput
            };
            telemetryBox.Paint += (s, e) =>
            {
                using (Pen borderPen = new Pen(Theme.Border, 1f))
                {
                    e.Graphics.DrawRectangle(borderPen, 0, 0, telemetryBox.Width - 1, telemetryBox.Height - 1);
                }
                using (SolidBrush bar = new SolidBrush(Theme.AccentAmber))
                {
                    e.Graphics.FillRectangle(bar, 0, 0, 3, telemetryBox.Height);
                }
            };

            _lblDiagTelemetry = new Label
            {
                Text = "Reading system telemetry...",
                Font = Theme.FontMonoSmall,
                ForeColor = Theme.TextSecondary,
                BackColor = Theme.BgInput,
                UseMnemonic = false,
                Location = new Point(14, 12),
                Size = new Size(660, 108)
            };
            telemetryBox.Controls.Add(_lblDiagTelemetry);

            _btnGenerateDiag = new ModernButton
            {
                Text = "GENERATE DIAGNOSTIC ZIP",
                Icon = IconType.Wrench,
                Width = 250,
                Height = 40,
                Location = new Point(16, 276),
                Variant = ButtonVariant.PrimaryCyan,
                Font = Theme.FontSubtitle
            };
            _btnGenerateDiag.Click += (s, e) => CollectDiagnostics();

            _btnOpenDiagFolder = new ModernButton
            {
                Text = "Open Folder",
                Icon = IconType.Folder,
                Width = 170,
                Height = 40,
                Location = new Point(278, 276),
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
                Text = "Status: Ready to generate diagnostic archive.",
                Font = Theme.FontMonoSmall,
                ForeColor = Theme.TextSecondary,
                BackColor = Theme.BgCard,
                UseMnemonic = false,
                Location = new Point(16, 332),
                AutoSize = true
            };

            card.Controls.Add(lblDesc);
            card.Controls.Add(telemetryBox);
            card.Controls.Add(_btnGenerateDiag);
            card.Controls.Add(_btnOpenDiagFolder);
            card.Controls.Add(_lblDiagStatus);

            _pageDiag.Controls.Add(card);
        }

        private void UpdateDiagnosticsTelemetry()
        {
            if (_lblDiagTelemetry == null) return;
            string gameNorm = ModDeployer.NormalizeGamePath(_tbGamePath != null ? _tbGamePath.Text : "");
            bool modReady = !string.IsNullOrEmpty(gameNorm) && ModDeployer.IsModInstalled(gameNorm);
            string logsDir = !string.IsNullOrEmpty(gameNorm) ? Path.Combine(gameNorm, @"_appdata_\logs") : "N/A";
            _lblDiagTelemetry.Text = string.Format(
                "PDA CALLSIGN  : {0}\n" +
                "LOCAL LAN IP  : {1}\n" +
                "GAME ROOT     : {2}\n" +
                "MOD STATUS    : {3}\n" +
                "LOGS PATH     : {4}\n" +
                "CONFIG INI    : {5}",
                _tbPlayerName != null ? _tbPlayerName.Text : Environment.UserName,
                _localIp,
                string.IsNullOrEmpty(gameNorm) ? "<not set>" : gameNorm,
                modReady ? "READY (OpenXRay x64 + coopnet)" : "NOT DEPLOYED",
                logsDir,
                LauncherConfig.GetConfigPath());
        }

        private void InitializeLogic()
        {
            _localIp = NetworkHelper.GetLocalIpAddress();
            _lblSidebarIp.Text = "LAN: " + _localIp;

            _config = LauncherConfig.Load();
            if (!string.IsNullOrEmpty(_config.PlayerName))
            {
                _tbPlayerName.Text = _config.PlayerName;
                _lblSidebarPlayer.Text = "Stalker: " + _config.PlayerName;
            }
            if (!string.IsNullOrEmpty(_config.DirectIp)) _tbDirectIp.Text = _config.DirectIp;
            if (!string.IsNullOrEmpty(_config.DirectPort)) _tbDirectPort.Text = _config.DirectPort;
            if (!string.IsNullOrEmpty(_config.HostServerName)) _tbHostServerName.Text = _config.HostServerName;
            if (!string.IsNullOrEmpty(_config.HostPlayerName)) _tbHostPlayerName.Text = _config.HostPlayerName;
            if (_config.HostPort >= _nudHostPort.Minimum && _config.HostPort <= _nudHostPort.Maximum)
                _nudHostPort.Value = _config.HostPort;
            _cbHostBroadcast.Checked = _config.HostBroadcast;
            _cbHostSkipIntro.Checked = _config.HostSkipIntro;
            _cbGlobalSkipIntro.Checked = _config.GlobalSkipIntro;

            if (_config.RecentServers != null && _config.RecentServers.Count > 0 && _btnQuickRecent != null)
            {
                string lastEntry = _config.RecentServers[0];
                _btnQuickRecent.Text = "Last: " + lastEntry;
                _btnQuickRecent.Visible = true;
                _btnQuickRecent.Click += (s, e) =>
                {
                    string[] p = lastEntry.Split(':');
                    if (p.Length >= 2)
                    {
                        _tbDirectIp.Text = p[0];
                        _tbDirectPort.Text = p[1];
                    }
                };
            }

            if (!string.IsNullOrEmpty(_config.GamePath) && Directory.Exists(_config.GamePath))
            {
                _gamePath = _config.GamePath;
                _tbGamePath.Text = _gamePath;
                UpdateModStatus();
            }
            else
            {
                _gamePath = GameLocator.DetectGamePath();
                if (!string.IsNullOrEmpty(_gamePath))
                {
                    _tbGamePath.Text = _gamePath;
                    UpdateModStatus();
                    SaveConfig();
                }
                else
                {
                    _lblSettingsModStatus.Text = "Game not automatically detected. Please browse for game folder.";
                    _lblSettingsModStatus.ForeColor = Theme.AccentAmber;
                }
            }

            _lblHostAddressCopy.Text = string.Format("Uplink Address: {0}:{1}", _localIp, _nudHostPort.Value);
            UpdateDiagnosticsTelemetry();

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
                _lblModBadge.Text = "NO GAME PATH";
                _lblModBadge.AccentColor = Theme.AccentRed;
                _lblModBadge.FillColor = Color.FromArgb(46, 16, 22);
                return;
            }

            if (ModDeployer.IsModInstalled(norm))
            {
                _lblSettingsModStatus.Text = "✔ Mod files installed (OpenXRay x64 binaries & coop scripts present)";
                _lblSettingsModStatus.ForeColor = Theme.AccentGreen;
                _lblModBadge.Text = "GAME & MOD READY";
                _lblModBadge.AccentColor = Theme.AccentGreen;
                _lblModBadge.FillColor = Color.FromArgb(14, 40, 32);
            }
            else
            {
                _lblSettingsModStatus.Text = "⚠ Mod binaries missing. Click 'DEPLOY MOD FILES' below.";
                _lblSettingsModStatus.ForeColor = Theme.AccentAmber;
                _lblModBadge.Text = "MOD MISSING";
                _lblModBadge.AccentColor = Theme.AccentAmber;
                _lblModBadge.FillColor = Color.FromArgb(44, 34, 14);
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
            _lvServers.Visible = true;

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

            if (_radarPanel != null) _radarPanel.BeaconCount = _lvServers.Items.Count;
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
                _lvServers.Visible = false;
            }

            if (_radarPanel != null) _radarPanel.BeaconCount = _lvServers.Items.Count;
        }

        private void RefreshLobbyServers()
        {
            if (_pages != null && _pages.Length > 0 && !_pageLobby.Visible)
            {
                SwitchPage(0);
            }

            _lvServers.BeginUpdate();
            _lvServers.Items.Clear();

            if (_beaconService != null)
            {
                List<LobbyServerInfo> active = _beaconService.GetActiveServers();
                if (active.Count > 0)
                {
                    _lblLobbyEmpty.Visible = false;
                    _lvServers.Visible = true;
                    foreach (var s in active)
                    {
                        OnLobbyServerUpdated(s);
                    }
                }
                else
                {
                    _lblLobbyEmpty.Visible = true;
                    _lvServers.Visible = false;
                    if (_radarPanel != null) _radarPanel.BeaconCount = 0;
                }
            }
            else
            {
                _lblLobbyEmpty.Visible = true;
                _lvServers.Visible = false;
                if (_radarPanel != null) _radarPanel.BeaconCount = 0;
            }
            _lvServers.EndUpdate();

            if (_btnRefreshLobby != null)
            {
                string originalText = "Scan";
                _btnRefreshLobby.Text = "Synced";
                Timer t = new Timer { Interval = 750 };
                t.Tick += (s, e) =>
                {
                    t.Stop();
                    t.Dispose();
                    if (_btnRefreshLobby != null && !_btnRefreshLobby.IsDisposed)
                        _btnRefreshLobby.Text = originalText;
                };
                t.Start();
            }
        }

        private void JoinSelectedServer()
        {
            if (_lvServers.SelectedItems.Count == 0) return;

            ListViewItem item = _lvServers.SelectedItems[0];
            string address = item.Tag as string;
            if (string.IsNullOrEmpty(address)) return;

            string[] parts = address.Split(':');
            if (parts.Length < 2) return;
            string ip = parts[0];
            int port;
            if (!int.TryParse(parts[1], out port)) return;

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
            SaveConfig();
            if (_config != null)
            {
                _config.AddRecentServer(ip, port);
                _config.Save();
            }

            string gamePath = _tbGamePath.Text;
            if (!ModDeployer.IsModInstalled(gamePath))
            {
                MessageBox.Show("Mod files are not installed in the game directory. Please install them first under Mod & Config tab.",
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
                _lblNetworkBadge.Text = string.Format("UPLINK: {0}:{1}", ip, port);
                _lblNetworkBadge.AccentColor = Theme.AccentCyan;
                _lblNetworkBadge.FillColor = Color.FromArgb(14, 42, 62);
            }
            else
            {
                MessageBox.Show("Failed to launch client: " + error, "Launch Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void StartHostingSession()
        {
            SaveConfig();

            string gamePath = _tbGamePath.Text;
            if (!ModDeployer.IsModInstalled(gamePath))
            {
                MessageBox.Show("Mod files are not installed in the game directory. Please install them first under Mod & Config tab.",
                    "Mod Not Installed", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                SwitchPage(2);
                return;
            }

            int port = _nudHostPort.Value;
            string serverName = _tbHostServerName.Text.Trim();
            string hostName = _tbHostPlayerName.Text.Trim();

            string error;
            bool skipIntro = (_cbHostSkipIntro != null && _cbHostSkipIntro.Checked) ||
                             (_cbGlobalSkipIntro != null && _cbGlobalSkipIntro.Checked);
            Process proc = SessionLauncher.StartHost(gamePath, port, skipIntro, out error);
            if (proc != null)
            {
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

                _lblHostLiveStatus.Text = string.Format("IN-GAME LOBBY OPEN on UDP port {0}. Configure skin & Ready in Clear Sky.", port);
                _lblHostLiveStatus.ForeColor = Theme.AccentGreen;
                _lblHostAddressCopy.Text = string.Format("Uplink Address: {0}:{1} (Share with partner)", _localIp, port);
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
                    SaveConfig();
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
                SaveConfig();
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
                _lblDiagStatus.Text = "Status: Collecting diagnostics (logs, dumps, saves)...";
                _lblDiagStatus.ForeColor = Theme.AccentAmber;
                Application.DoEvents();

                string baseDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
                if (string.IsNullOrEmpty(baseDir) || !Directory.Exists(baseDir)) baseDir = AppDomain.CurrentDomain.BaseDirectory;
                string diagDir = Path.Combine(baseDir, "diagnostics");
                if (!Directory.Exists(diagDir)) Directory.CreateDirectory(diagDir);

                string timestamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");
                string packName = "coop_diag_" + timestamp;
                string packDir = Path.Combine(diagDir, packName);
                if (Directory.Exists(packDir)) Directory.Delete(packDir, true);
                Directory.CreateDirectory(packDir);

                List<string> sysInfo = new List<string>();
                sysInfo.Add("=================================================");
                sysInfo.Add("  S.T.A.L.K.E.R. Clear Sky Co-op Diagnostic Pack");
                sysInfo.Add("=================================================");
                sysInfo.Add("Generated at: " + DateTime.UtcNow.ToString("o") + " (UTC)");
                sysInfo.Add("OS Version: " + Environment.OSVersion.VersionString + (Environment.Is64BitOperatingSystem ? " (64-bit)" : " (32-bit)"));
                sysInfo.Add("Machine Name: " + Environment.MachineName);
                sysInfo.Add("User Name: " + Environment.UserName);
                sysInfo.Add("Processor Count: " + Environment.ProcessorCount);
                sysInfo.Add("CLR Version: " + Environment.Version);
                sysInfo.Add("Launcher Executable: " + Assembly.GetExecutingAssembly().Location);
                sysInfo.Add("Configured Game Path: " + (_tbGamePath != null ? _tbGamePath.Text : ""));
                sysInfo.Add("Direct Connect IP: " + (_tbDirectIp != null ? _tbDirectIp.Text : ""));
                sysInfo.Add("Direct Connect Port: " + (_tbDirectPort != null ? _tbDirectPort.Text : ""));
                sysInfo.Add("Host Server Name: " + (_tbHostServerName != null ? _tbHostServerName.Text : ""));
                sysInfo.Add("Host Player Name: " + (_tbHostPlayerName != null ? _tbHostPlayerName.Text : ""));
                sysInfo.Add("Host Port: " + (_nudHostPort != null ? _nudHostPort.Value.ToString() : ""));
                sysInfo.Add("Local LAN IP: " + _localIp);
                File.WriteAllLines(Path.Combine(packDir, "system_info.txt"), sysInfo.ToArray());

                string configPath = LauncherConfig.GetConfigPath();
                if (File.Exists(configPath))
                {
                    File.Copy(configPath, Path.Combine(packDir, "launcher_config.ini"), true);
                }

                List<string> candidateDirs = new List<string>();
                string configuredGame = ModDeployer.NormalizeGamePath(_tbGamePath != null ? _tbGamePath.Text : "");
                if (!string.IsNullOrEmpty(configuredGame) && Directory.Exists(configuredGame))
                    candidateDirs.Add(configuredGame);

                string localHost = Path.Combine(baseDir, @"local\game-host");
                if (Directory.Exists(localHost) && !candidateDirs.Contains(localHost)) candidateDirs.Add(localHost);
                string localClient = Path.Combine(baseDir, @"local\game-client");
                if (Directory.Exists(localClient) && !candidateDirs.Contains(localClient)) candidateDirs.Add(localClient);

                string repoRoot = Path.GetFullPath(Path.Combine(baseDir, @"..\.."));
                string repoHost = Path.Combine(repoRoot, @"local\game-host");
                if (Directory.Exists(repoHost) && !candidateDirs.Contains(repoHost)) candidateDirs.Add(repoHost);
                string repoClient = Path.Combine(repoRoot, @"local\game-client");
                if (Directory.Exists(repoClient) && !candidateDirs.Contains(repoClient)) candidateDirs.Add(repoClient);

                int collectedCount = 0;
                foreach (string rootPath in candidateDirs)
                {
                    string targetSub = Path.Combine(packDir, Path.GetFileName(rootPath.TrimEnd('\\', '/')));
                    if (!Directory.Exists(targetSub)) Directory.CreateDirectory(targetSub);

                    string fsgame = Path.Combine(rootPath, "fsgame.ltx");
                    if (File.Exists(fsgame))
                    {
                        File.Copy(fsgame, Path.Combine(targetSub, "fsgame.ltx"), true);
                    }

                    string appData = Path.Combine(rootPath, "_appdata_");
                    if (Directory.Exists(appData))
                    {
                        string userLtx = Path.Combine(appData, "user.ltx");
                        if (File.Exists(userLtx))
                        {
                            File.Copy(userLtx, Path.Combine(targetSub, "user.ltx"), true);
                        }

                        string logsDir = Path.Combine(appData, "logs");
                        if (Directory.Exists(logsDir))
                        {
                            string destLogs = Path.Combine(targetSub, "logs");
                            Directory.CreateDirectory(destLogs);
                            var logFiles = new DirectoryInfo(logsDir).GetFiles("*.log");
                            Array.Sort(logFiles, (a, b) => b.LastWriteTimeUtc.CompareTo(a.LastWriteTimeUtc));
                            int count = Math.Min(logFiles.Length, 5);
                            for (int i = 0; i < count; i++)
                            {
                                File.Copy(logFiles[i].FullName, Path.Combine(destLogs, logFiles[i].Name), true);
                                collectedCount++;
                            }
                        }

                        string dumpsDir = Path.Combine(appData, "dumps");
                        if (Directory.Exists(dumpsDir))
                        {
                            string destDumps = Path.Combine(targetSub, "dumps");
                            Directory.CreateDirectory(destDumps);
                            var dumpFiles = new DirectoryInfo(dumpsDir).GetFiles("*.mdmp");
                            Array.Sort(dumpFiles, (a, b) => b.LastWriteTimeUtc.CompareTo(a.LastWriteTimeUtc));
                            int count = Math.Min(dumpFiles.Length, 3);
                            for (int i = 0; i < count; i++)
                            {
                                File.Copy(dumpFiles[i].FullName, Path.Combine(destDumps, dumpFiles[i].Name), true);
                                collectedCount++;
                            }
                        }

                        string savesDir = Path.Combine(appData, "savedgames");
                        if (Directory.Exists(savesDir))
                        {
                            string destSaves = Path.Combine(targetSub, "savedgames");
                            Directory.CreateDirectory(destSaves);
                            var saveFiles = new DirectoryInfo(savesDir).GetFiles("*.sav");
                            Array.Sort(saveFiles, (a, b) => b.LastWriteTimeUtc.CompareTo(a.LastWriteTimeUtc));
                            int count = Math.Min(saveFiles.Length, 2);
                            for (int i = 0; i < count; i++)
                            {
                                File.Copy(saveFiles[i].FullName, Path.Combine(destSaves, saveFiles[i].Name), true);
                                collectedCount++;
                            }
                        }
                    }
                }

                string scriptPath = Path.Combine(baseDir, @"scripts\collect_diagnostics.ps1");
                if (!File.Exists(scriptPath)) scriptPath = Path.Combine(repoRoot, @"scripts\collect_diagnostics.ps1");
                if (File.Exists(scriptPath))
                {
                    try
                    {
                        ProcessStartInfo psi = new ProcessStartInfo
                        {
                            FileName = "powershell.exe",
                            Arguments = string.Format("-NoProfile -ExecutionPolicy Bypass -File \"{0}\" -OutDir \"{1}\"", scriptPath, packDir),
                            UseShellExecute = false,
                            CreateNoWindow = true
                        };
                        Process p = Process.Start(psi);
                        if (p != null) p.WaitForExit();
                    }
                    catch { }
                }

                string zipPath = Path.Combine(diagDir, packName + ".zip");
                if (File.Exists(zipPath)) File.Delete(zipPath);

                ZipFile.CreateFromDirectory(packDir, zipPath, CompressionLevel.Optimal, false);

                try { Directory.Delete(packDir, true); } catch { }

                _lblDiagStatus.Text = string.Format("Status: ✔ Diagnostic pack generated: {0}.zip ({1} files)", packName, collectedCount);
                _lblDiagStatus.ForeColor = Theme.AccentGreen;
                _btnOpenDiagFolder.Visible = true;

                try
                {
                    Process.Start("explorer.exe", string.Format("/select,\"{0}\"", zipPath));
                }
                catch
                {
                    Process.Start("explorer.exe", diagDir);
                }
            }
            catch (Exception ex)
            {
                _lblDiagStatus.Text = "Status: Error collecting diagnostics: " + ex.Message;
                _lblDiagStatus.ForeColor = Theme.AccentRed;
            }
        }

        private void ScheduleAutoSave()
        {
            if (_saveDebounceTimer == null)
            {
                _saveDebounceTimer = new Timer { Interval = 500 };
                _saveDebounceTimer.Tick += (s, e) =>
                {
                    _saveDebounceTimer.Stop();
                    SaveConfig();
                };
            }
            _saveDebounceTimer.Stop();
            _saveDebounceTimer.Start();
        }

        private void SaveConfig()
        {
            try
            {
                if (_config == null) _config = new LauncherConfig();
                if (_tbPlayerName != null) _config.PlayerName = _tbPlayerName.Text.Trim();
                if (_tbGamePath != null) _config.GamePath = _tbGamePath.Text.Trim();
                if (_tbDirectIp != null) _config.DirectIp = _tbDirectIp.Text.Trim();
                if (_tbDirectPort != null) _config.DirectPort = _tbDirectPort.Text.Trim();
                if (_tbHostServerName != null) _config.HostServerName = _tbHostServerName.Text.Trim();
                if (_tbHostPlayerName != null) _config.HostPlayerName = _tbHostPlayerName.Text.Trim();
                if (_nudHostPort != null) _config.HostPort = _nudHostPort.Value;
                if (_cbHostBroadcast != null) _config.HostBroadcast = _cbHostBroadcast.Checked;
                if (_cbHostSkipIntro != null) _config.HostSkipIntro = _cbHostSkipIntro.Checked;
                if (_cbGlobalSkipIntro != null) _config.GlobalSkipIntro = _cbGlobalSkipIntro.Checked;
                _config.Save();
            }
            catch { }
        }

        private void ConfigureFirewall()
        {
            try
            {
                string launcherExe = Assembly.GetExecutingAssembly().Location;
                string gamePath = ModDeployer.NormalizeGamePath(_tbGamePath != null ? _tbGamePath.Text : "");
                string xrEngineExe = (!string.IsNullOrEmpty(gamePath) && Directory.Exists(gamePath))
                    ? Path.Combine(gamePath, @"bin\x64\xrEngine.exe")
                    : "";

                List<string> cmds = new List<string>();
                cmds.Add("netsh advfirewall firewall delete rule name=\"Clear Sky Coop UDP\"");
                cmds.Add("netsh advfirewall firewall delete rule name=\"Clear Sky Coop xrEngine\"");
                cmds.Add("netsh advfirewall firewall delete rule name=\"Clear Sky Coop Launcher\"");
                cmds.Add("netsh advfirewall firewall add rule name=\"Clear Sky Coop UDP\" dir=in action=allow protocol=UDP localport=27015-27016 profile=any");
                cmds.Add("netsh advfirewall firewall add rule name=\"Clear Sky Coop UDP\" dir=out action=allow protocol=UDP localport=27015-27016 profile=any");

                if (File.Exists(launcherExe))
                {
                    cmds.Add(string.Format("netsh advfirewall firewall add rule name=\"Clear Sky Coop Launcher\" dir=in action=allow program=\"{0}\" enable=yes profile=any", launcherExe));
                }

                if (File.Exists(xrEngineExe))
                {
                    cmds.Add(string.Format("netsh advfirewall firewall add rule name=\"Clear Sky Coop xrEngine\" dir=in action=allow program=\"{0}\" enable=yes profile=any", xrEngineExe));
                }

                string combinedArgs = "/c " + string.Join(" & ", cmds.ToArray());

                ProcessStartInfo psi = new ProcessStartInfo
                {
                    FileName = "cmd.exe",
                    Arguments = combinedArgs,
                    Verb = "runas",
                    UseShellExecute = true,
                    WindowStyle = ProcessWindowStyle.Hidden
                };

                Process p = Process.Start(psi);
                if (p != null)
                {
                    p.WaitForExit();
                    MessageBox.Show("Windows Firewall rules configured successfully!\n\n" +
                                    "• UDP ports 27015-27016 allowed for Inbound and Outbound\n" +
                                    "• Clear Sky Co-op Launcher allowed for LAN beacon discovery\n" +
                                    (File.Exists(xrEngineExe) ? "• xrEngine.exe allowed for co-op game traffic\n" : ""),
                                    "Windows Firewall Setup", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("Could not configure firewall (requires Administrator elevation):\n" + ex.Message,
                    "Firewall Setup", MessageBoxButtons.OK, MessageBoxIcon.Warning);
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
            SaveConfig();
            if (_beaconService != null)
            {
                _beaconService.Dispose();
            }
            base.OnFormClosing(e);
        }
    }
}
