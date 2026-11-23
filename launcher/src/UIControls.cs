using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace ClearSkyCoop.Launcher
{
    public enum ButtonVariant
    {
        PrimaryCyan,
        PrimaryGreen,
        Secondary,
        Danger,
        Nav
    }

    public class ModernButton : Button
    {
        private bool _isHovered = false;
        private bool _isPressed = false;
        private bool _isActiveNav = false;

        public ButtonVariant Variant { get; set; } = ButtonVariant.Secondary;
        public int BorderRadius { get; set; } = 6;
        public string SubText { get; set; } = null;
        public string BadgeText { get; set; } = null;

        public bool IsActiveNav
        {
            get { return _isActiveNav; }
            set { _isActiveNav = value; Invalidate(); }
        }

        public ModernButton()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.UserPaint |
                     ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.ResizeRedraw, true);

            this.FlatStyle = FlatStyle.Flat;
            this.FlatAppearance.BorderSize = 0;
            this.Cursor = Cursors.Hand;
            this.Font = Theme.FontRegularBold;
            this.ForeColor = Theme.TextPrimary;
            this.Height = 36;
        }

        protected override void OnMouseEnter(EventArgs e)
        {
            _isHovered = true;
            Invalidate();
            base.OnMouseEnter(e);
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            _isHovered = false;
            _isPressed = false;
            Invalidate();
            base.OnMouseLeave(e);
        }

        protected override void OnMouseDown(MouseEventArgs mevent)
        {
            _isPressed = true;
            Invalidate();
            base.OnMouseDown(mevent);
        }

        protected override void OnMouseUp(MouseEventArgs mevent)
        {
            _isPressed = false;
            Invalidate();
            base.OnMouseUp(mevent);
        }

        protected override void OnPaint(PaintEventArgs pevent)
        {
            Graphics g = pevent.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

            Rectangle rect = new Rectangle(0, 0, Width - 1, Height - 1);

            Color bg;
            Color borderColor = Theme.Border;
            Color textColor = Theme.TextPrimary;

            switch (Variant)
            {
                case ButtonVariant.PrimaryCyan:
                    bg = _isPressed ? Color.FromArgb(0, 130, 204) : (_isHovered ? Theme.AccentCyanHover : Theme.AccentCyan);
                    borderColor = bg;
                    textColor = Color.White;
                    break;

                case ButtonVariant.PrimaryGreen:
                    bg = _isPressed ? Color.FromArgb(4, 120, 87) : (_isHovered ? Theme.AccentGreenHover : Theme.AccentGreen);
                    borderColor = bg;
                    textColor = Color.White;
                    break;

                case ButtonVariant.Danger:
                    bg = _isPressed ? Color.FromArgb(185, 28, 28) : (_isHovered ? Theme.AccentRedHover : Theme.AccentRed);
                    borderColor = bg;
                    textColor = Color.White;
                    break;

                case ButtonVariant.Nav:
                    if (_isActiveNav)
                    {
                        bg = Theme.BgCardHover;
                        borderColor = Theme.Border;
                        textColor = Theme.TextPrimary;
                    }
                    else if (_isHovered)
                    {
                        bg = Color.FromArgb(28, 33, 40);
                        borderColor = Color.Transparent;
                        textColor = Theme.TextPrimary;
                    }
                    else
                    {
                        bg = Color.Transparent;
                        borderColor = Color.Transparent;
                        textColor = Theme.TextSecondary;
                    }
                    break;

                case ButtonVariant.Secondary:
                default:
                    bg = _isPressed ? Color.FromArgb(22, 26, 32) : (_isHovered ? Theme.BgCardHover : Theme.BgCard);
                    borderColor = _isHovered ? Theme.BorderLight : Theme.Border;
                    textColor = _isHovered ? Color.White : Theme.TextPrimary;
                    break;
            }

            if (!Enabled)
            {
                bg = Color.FromArgb(30, 34, 40);
                borderColor = Color.FromArgb(40, 45, 55);
                textColor = Theme.TextMuted;
            }

            // Fill background
            using (GraphicsPath path = Theme.CreateRoundedPath(rect, BorderRadius))
            {
                using (SolidBrush brush = new SolidBrush(bg))
                {
                    g.FillPath(brush, path);
                }

                if (borderColor != Color.Transparent)
                {
                    using (Pen pen = new Pen(borderColor, 1f))
                    {
                        g.DrawPath(pen, path);
                    }
                }
            }

            // Draw Nav left indicator
            if (Variant == ButtonVariant.Nav && _isActiveNav)
            {
                using (SolidBrush accentBrush = new SolidBrush(Theme.AccentCyan))
                {
                    g.FillRectangle(accentBrush, 0, 4, 4, Height - 8);
                }
            }

            // Draw Text
            StringFormat sf = new StringFormat
            {
                Alignment = (Variant == ButtonVariant.Nav) ? StringAlignment.Near : StringAlignment.Center,
                LineAlignment = StringAlignment.Center
            };

            int textX = (Variant == ButtonVariant.Nav) ? 18 : 0;
            Rectangle textRect = new Rectangle(textX, 0, Width - textX - (string.IsNullOrEmpty(BadgeText) ? 0 : 45), Height);

            using (SolidBrush textBrush = new SolidBrush(textColor))
            {
                g.DrawString(Text, Font, textBrush, textRect, sf);
            }

            // Optional badge
            if (!string.IsNullOrEmpty(BadgeText))
            {
                int badgeX = Width - 42;
                int badgeY = (Height - 16) / 2;
                Theme.DrawBadge(g, BadgeText, badgeX, badgeY, Color.FromArgb(40, 60, 85), Theme.AccentCyan);
            }
        }
    }

    public class ModernCard : Panel
    {
        public string Title { get; set; }
        public string Subtitle { get; set; }
        public int BorderRadius { get; set; } = 8;
        public Color BorderColor { get; set; } = Theme.Border;
        public Color CardBackground { get; set; } = Theme.BgCard;

        public ModernCard()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.UserPaint |
                     ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.ResizeRedraw, true);

            this.BackColor = Theme.BgDarkest;
            this.Padding = new Padding(16);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

            Rectangle rect = new Rectangle(0, 0, Width - 1, Height - 1);

            using (GraphicsPath path = Theme.CreateRoundedPath(rect, BorderRadius))
            {
                using (SolidBrush brush = new SolidBrush(CardBackground))
                {
                    g.FillPath(brush, path);
                }
                using (Pen pen = new Pen(BorderColor, 1f))
                {
                    g.DrawPath(pen, path);
                }
            }

            // Draw Card Title if provided
            int headerOffset = 14;
            if (!string.IsNullOrEmpty(Title))
            {
                using (SolidBrush titleBrush = new SolidBrush(Theme.TextPrimary))
                {
                    g.DrawString(Title, Theme.FontSubtitle, titleBrush, 16, headerOffset);
                }
                headerOffset += 20;

                if (!string.IsNullOrEmpty(Subtitle))
                {
                    using (SolidBrush subBrush = new SolidBrush(Theme.TextMuted))
                    {
                        g.DrawString(Subtitle, Theme.FontSmall, subBrush, 16, headerOffset);
                    }
                }
            }

            base.OnPaint(e);
        }
    }

    public class ModernTextBox : UserControl
    {
        private TextBox _innerBox;
        private bool _isFocused = false;
        private string _placeholder = "";

        public string Placeholder
        {
            get { return _placeholder; }
            set { _placeholder = value; Invalidate(); }
        }

        public override string Text
        {
            get { return _innerBox.Text; }
            set { _innerBox.Text = value; Invalidate(); }
        }

        public new event EventHandler TextChanged;

        public ModernTextBox()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.UserPaint |
                     ControlStyles.OptimizedDoubleBuffer, true);

            this.BackColor = Theme.BgInput;
            this.Height = 36;
            this.Padding = new Padding(10, 8, 10, 8);

            _innerBox = new TextBox
            {
                BorderStyle = BorderStyle.None,
                BackColor = Theme.BgInput,
                ForeColor = Theme.TextPrimary,
                Font = Theme.FontRegular,
                Dock = DockStyle.Fill
            };

            _innerBox.GotFocus += (s, e) => { _isFocused = true; Invalidate(); };
            _innerBox.LostFocus += (s, e) => { _isFocused = false; Invalidate(); };
            _innerBox.TextChanged += (s, e) => { if (TextChanged != null) TextChanged(this, e); Invalidate(); };

            this.Controls.Add(_innerBox);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;

            Rectangle rect = new Rectangle(0, 0, Width - 1, Height - 1);
            Color borderColor = _isFocused ? Theme.BorderFocus : Theme.Border;

            using (GraphicsPath path = Theme.CreateRoundedPath(rect, 5))
            {
                using (SolidBrush brush = new SolidBrush(Theme.BgInput))
                {
                    g.FillPath(brush, path);
                }
                using (Pen pen = new Pen(borderColor, _isFocused ? 1.5f : 1f))
                {
                    g.DrawPath(pen, path);
                }
            }

            // Draw placeholder if empty
            if (string.IsNullOrEmpty(_innerBox.Text) && !_isFocused && !string.IsNullOrEmpty(_placeholder))
            {
                using (SolidBrush hintBrush = new SolidBrush(Theme.TextMuted))
                {
                    g.DrawString(_placeholder, Theme.FontRegular, hintBrush, 12, (Height - 16) / 2);
                }
            }

            base.OnPaint(e);
        }
    }

    public class TitleBarControl : Panel
    {
        [DllImport("user32.dll")]
        public static extern int SendMessage(IntPtr hWnd, int Msg, int wParam, int lParam);
        [DllImport("user32.dll")]
        public static extern bool ReleaseCapture();

        public const int WM_NCLBUTTONDOWN = 0xA1;
        public const int HT_CAPTION = 0x2;

        private Form _parentForm;
        private Label _lblClose;
        private Label _lblMinimize;

        public TitleBarControl(Form parent)
        {
            _parentForm = parent;
            this.Dock = DockStyle.Top;
            this.Height = 36;
            this.BackColor = Theme.BgSidebar;

            // Window controls on right
            _lblClose = new Label
            {
                Text = "✕",
                Font = new Font("Segoe UI", 9f, FontStyle.Regular),
                ForeColor = Theme.TextSecondary,
                Size = new Size(42, 36),
                TextAlign = ContentAlignment.MiddleCenter,
                Dock = DockStyle.Right,
                Cursor = Cursors.Hand
            };
            _lblClose.MouseEnter += (s, e) => { _lblClose.BackColor = Theme.AccentRed; _lblClose.ForeColor = Color.White; };
            _lblClose.MouseLeave += (s, e) => { _lblClose.BackColor = Color.Transparent; _lblClose.ForeColor = Theme.TextSecondary; };
            _lblClose.Click += (s, e) => _parentForm.Close();

            _lblMinimize = new Label
            {
                Text = "—",
                Font = new Font("Segoe UI", 9f, FontStyle.Regular),
                ForeColor = Theme.TextSecondary,
                Size = new Size(42, 36),
                TextAlign = ContentAlignment.MiddleCenter,
                Dock = DockStyle.Right,
                Cursor = Cursors.Hand
            };
            _lblMinimize.MouseEnter += (s, e) => { _lblMinimize.BackColor = Theme.BgCardHover; _lblMinimize.ForeColor = Color.White; };
            _lblMinimize.MouseLeave += (s, e) => { _lblMinimize.BackColor = Color.Transparent; _lblMinimize.ForeColor = Theme.TextSecondary; };
            _lblMinimize.Click += (s, e) => _parentForm.WindowState = FormWindowState.Minimized;

            this.Controls.Add(_lblMinimize);
            this.Controls.Add(_lblClose);

            this.MouseDown += OnTitleBarMouseDown;
        }

        private void OnTitleBarMouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Left)
            {
                ReleaseCapture();
                SendMessage(_parentForm.Handle, WM_NCLBUTTONDOWN, HT_CAPTION, 0);
            }
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

            // Faction Logo / Icon (Cyan Tactical Diamond)
            Point[] diamond = new Point[]
            {
                new Point(20, 10),
                new Point(27, 18),
                new Point(20, 26),
                new Point(13, 18)
            };
            using (SolidBrush logoBrush = new SolidBrush(Theme.AccentCyan))
            {
                g.FillPolygon(logoBrush, diamond);
            }

            // Title
            using (SolidBrush textBrush = new SolidBrush(Theme.TextPrimary))
            {
                g.DrawString("S.T.A.L.K.E.R. CLEAR SKY", Theme.FontBadge, textBrush, 36, 11);
            }

            // Tag badge
            Theme.DrawBadge(g, "CO-OP v1.0", 175, 9, Color.FromArgb(20, 48, 70), Theme.AccentCyan);

            // Subtle bottom line
            using (Pen borderPen = new Pen(Theme.Border, 1f))
            {
                g.DrawLine(borderPen, 0, Height - 1, Width, Height - 1);
            }

            base.OnPaint(e);
        }
    }
}
