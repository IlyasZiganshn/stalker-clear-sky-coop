using System;
using System.Collections.Generic;
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

        public ButtonVariant Variant { get; set; }
        public IconType Icon { get; set; }
        public int BorderRadius { get; set; }
        public string SubText { get; set; }
        public string BadgeText { get; set; }

        public bool IsActiveNav
        {
            get { return _isActiveNav; }
            set { _isActiveNav = value; Invalidate(); }
        }

        public ModernButton()
        {
            Variant = ButtonVariant.Secondary;
            Icon = IconType.None;
            BorderRadius = 6;
            SubText = null;
            BadgeText = null;

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
            this.UseMnemonic = false;
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

            // Clear parent surface first so rounded corners stay crisp
            Color parentBg = (Parent != null) ? Parent.BackColor : Theme.BgCard;
            using (SolidBrush parentBrush = new SolidBrush(parentBg))
            {
                g.FillRectangle(parentBrush, ClientRectangle);
            }

            Rectangle rect = new Rectangle(0, 0, Width - 1, Height - 1);

            Color bgTop = Theme.BgCardElevated;
            Color bgBottom = Theme.BgCardElevated;
            Color borderColor = Theme.Border;
            Color textColor = Theme.TextPrimary;
            bool useGradient = false;

            switch (Variant)
            {
                case ButtonVariant.PrimaryCyan:
                    useGradient = true;
                    if (_isPressed)
                    {
                        bgTop = Color.FromArgb(0, 108, 185);
                        bgBottom = Color.FromArgb(0, 82, 148);
                    }
                    else if (_isHovered)
                    {
                        bgTop = Theme.AccentCyanHover;
                        bgBottom = Theme.AccentCyan;
                    }
                    else
                    {
                        bgTop = Theme.AccentCyan;
                        bgBottom = Theme.AccentCyanDark;
                    }
                    borderColor = _isHovered ? Color.FromArgb(140, 226, 255) : Theme.AccentCyanDark;
                    textColor = Color.White;
                    break;

                case ButtonVariant.PrimaryGreen:
                    useGradient = true;
                    if (_isPressed)
                    {
                        bgTop = Color.FromArgb(4, 118, 84);
                        bgBottom = Color.FromArgb(3, 88, 62);
                    }
                    else if (_isHovered)
                    {
                        bgTop = Theme.AccentGreenHover;
                        bgBottom = Theme.AccentGreen;
                    }
                    else
                    {
                        bgTop = Theme.AccentGreen;
                        bgBottom = Theme.AccentGreenDark;
                    }
                    borderColor = _isHovered ? Color.FromArgb(115, 245, 188) : Theme.AccentGreenDark;
                    textColor = Color.White;
                    break;

                case ButtonVariant.Danger:
                    bgTop = _isPressed ? Color.FromArgb(185, 28, 28) : (_isHovered ? Theme.AccentRedHover : Theme.AccentRed);
                    bgBottom = bgTop;
                    borderColor = bgTop;
                    textColor = Color.White;
                    break;

                case ButtonVariant.Nav:
                    if (_isActiveNav)
                    {
                        bgTop = Color.FromArgb(22, 34, 52);
                        bgBottom = Color.FromArgb(18, 27, 42);
                        borderColor = Color.FromArgb(42, 72, 106);
                        textColor = Theme.TextPrimary;
                        useGradient = true;
                    }
                    else if (_isHovered)
                    {
                        bgTop = Color.FromArgb(20, 28, 40);
                        bgBottom = bgTop;
                        borderColor = Color.FromArgb(32, 44, 62);
                        textColor = Theme.TextPrimary;
                    }
                    else
                    {
                        bgTop = Theme.BgSidebar;
                        bgBottom = Theme.BgSidebar;
                        borderColor = Color.Transparent;
                        textColor = Theme.TextSecondary;
                    }
                    break;

                case ButtonVariant.Secondary:
                default:
                    bgTop = _isPressed ? Color.FromArgb(16, 22, 32) : (_isHovered ? Theme.BgCardHover : Theme.BgCardElevated);
                    bgBottom = bgTop;
                    borderColor = _isHovered ? Theme.AccentCyan : Theme.BorderLight;
                    textColor = _isHovered ? Color.White : Theme.TextPrimary;
                    break;
            }

            if (!Enabled)
            {
                bgTop = Color.FromArgb(22, 27, 36);
                bgBottom = bgTop;
                borderColor = Color.FromArgb(34, 42, 56);
                textColor = Theme.TextMuted;
                useGradient = false;
            }

            using (GraphicsPath path = Theme.CreateRoundedPath(rect, BorderRadius))
            {
                if (useGradient && bgTop != bgBottom)
                {
                    using (LinearGradientBrush lgb = new LinearGradientBrush(rect, bgTop, bgBottom, LinearGradientMode.Vertical))
                    {
                        g.FillPath(lgb, path);
                    }
                }
                else
                {
                    using (SolidBrush brush = new SolidBrush(bgTop))
                    {
                        g.FillPath(brush, path);
                    }
                }

                if (borderColor != Color.Transparent)
                {
                    using (Pen pen = new Pen(borderColor, (_isHovered && useGradient) ? 1.4f : 1.0f))
                    {
                        g.DrawPath(pen, path);
                    }
                }
            }

            if (useGradient && Enabled && rect.Height > 10)
            {
                using (Pen topHighlight = new Pen(Color.FromArgb(65, 255, 255, 255), 1f))
                {
                    g.DrawLine(topHighlight, rect.Left + BorderRadius, rect.Top + 1, rect.Right - BorderRadius, rect.Top + 1);
                }
            }

            if (Variant == ButtonVariant.Nav && _isActiveNav)
            {
                using (SolidBrush accentBrush = new SolidBrush(Theme.AccentCyan))
                {
                    g.FillRectangle(accentBrush, 1, 7, 3, Height - 14);
                }
            }

            int textX = 0;
            if (Variant == ButtonVariant.Nav)
            {
                textX = 40;
                if (Icon != IconType.None)
                {
                    Rectangle iconRect = new Rectangle(14, (Height - 16) / 2, 16, 16);
                    Color iconColor = _isActiveNav ? Theme.AccentCyan : (_isHovered ? Theme.TextPrimary : Theme.TextSecondary);
                    Theme.DrawVectorIcon(g, Icon, iconRect, iconColor);
                }
            }
            else if (Icon != IconType.None)
            {
                SizeF textSize = g.MeasureString(Text, Font);
                int totalContentWidth = 16 + 8 + (int)textSize.Width;
                int startX = Math.Max(10, (Width - totalContentWidth) / 2);
                Rectangle iconRect = new Rectangle(startX, (Height - 16) / 2, 16, 16);
                Theme.DrawVectorIcon(g, Icon, iconRect, textColor);
                textX = startX + 22;
            }

            StringFormat sf = new StringFormat
            {
                Alignment = (Variant == ButtonVariant.Nav || Icon != IconType.None) ? StringAlignment.Near : StringAlignment.Center,
                LineAlignment = StringAlignment.Center
            };

            int rightReserve = string.IsNullOrEmpty(BadgeText) ? 0 : 40;
            Rectangle textRect = new Rectangle(textX, 0, Math.Max(10, Width - textX - rightReserve), Height);

            using (SolidBrush textBrush = new SolidBrush(textColor))
            {
                g.DrawString(Text, Font, textBrush, textRect, sf);
            }

            if (!string.IsNullOrEmpty(BadgeText))
            {
                SizeF badgeSz = g.MeasureString(BadgeText, Theme.FontMonoSmall);
                int bw = (int)Math.Ceiling(badgeSz.Width) + 10;
                int bx = Width - bw - 10;
                int by = (Height - 18) / 2;
                Color bBg = _isActiveNav ? Color.FromArgb(14, 52, 76) : Color.FromArgb(24, 32, 45);
                Color bFg = _isActiveNav ? Theme.AccentCyan : Theme.TextMuted;
                using (GraphicsPath bp = Theme.CreateRoundedPath(new Rectangle(bx, by, bw, 18), 4))
                using (SolidBrush bb = new SolidBrush(bBg))
                using (SolidBrush bt = new SolidBrush(bFg))
                {
                    g.FillPath(bb, bp);
                    g.DrawString(BadgeText, Theme.FontMonoSmall, bt, bx + 5, by + 2);
                }
            }
        }
    }

    public class ModernCard : Panel
    {
        private string _title = "";
        private string _subtitle = "";
        private string _headerTag = "";

        public string Title
        {
            get { return _title; }
            set { _title = value; UpdateHeaderPadding(); Invalidate(); }
        }

        public string Subtitle
        {
            get { return _subtitle; }
            set { _subtitle = value; UpdateHeaderPadding(); Invalidate(); }
        }

        public string HeaderTag
        {
            get { return _headerTag; }
            set { _headerTag = value; Invalidate(); }
        }

        public int BorderRadius { get; set; }
        public Color BorderColor { get; set; }
        public Color CardBackground { get; set; }
        public Color TopAccentColor { get; set; }
        public bool ShowCornerReticles { get; set; }

        public int HeaderHeight
        {
            get
            {
                if (!string.IsNullOrEmpty(_subtitle)) return 64;
                if (!string.IsNullOrEmpty(_title)) return 44;
                return 14;
            }
        }

        public ModernCard()
        {
            BorderRadius = 8;
            BorderColor = Theme.Border;
            CardBackground = Theme.BgCard;
            TopAccentColor = Color.Transparent;
            ShowCornerReticles = true;

            SetStyle(ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.UserPaint |
                     ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.ResizeRedraw, true);

            this.BackColor = Theme.BgCard;
            UpdateHeaderPadding();
        }

        private void UpdateHeaderPadding()
        {
            this.Padding = new Padding(16, HeaderHeight, 16, 14);
        }

        protected override void OnPaintBackground(PaintEventArgs e)
        {
            Color outer = (Parent != null) ? Parent.BackColor : Theme.BgDarkest;
            using (SolidBrush bg = new SolidBrush(outer))
            {
                e.Graphics.FillRectangle(bg, ClientRectangle);
            }
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

            // Liquid glass subtle top specular sheen
            using (Pen sheenPen = new Pen(Color.FromArgb(22, 255, 255, 255), 1f))
            {
                g.DrawLine(sheenPen, BorderRadius + 2, 1, Width - BorderRadius - 3, 1);
            }

            // Top tactical accent strip
            if (TopAccentColor != Color.Transparent)
            {
                using (Pen accentPen = new Pen(TopAccentColor, 2f))
                {
                    g.DrawLine(accentPen, BorderRadius + 4, 1, Width - BorderRadius - 5, 1);
                }
            }

            if (ShowCornerReticles)
            {
                Color retCol = (TopAccentColor != Color.Transparent)
                    ? Color.FromArgb(145, TopAccentColor)
                    : Color.FromArgb(130, Theme.BorderLight);
                Theme.DrawCornerReticles(g, new Rectangle(3, 3, Width - 7, Height - 7), retCol, 6);
            }

            int headerOffset = 12;
            if (!string.IsNullOrEmpty(_title))
            {
                using (SolidBrush titleBrush = new SolidBrush(Theme.TextPrimary))
                {
                    g.DrawString(_title, Theme.FontSubtitle, titleBrush, 16, headerOffset);
                }
                headerOffset += 20;

                if (!string.IsNullOrEmpty(_subtitle))
                {
                    using (SolidBrush subBrush = new SolidBrush(Theme.TextMuted))
                    {
                        g.DrawString(_subtitle, Theme.FontSmall, subBrush, 16, headerOffset);
                    }
                }

                if (!string.IsNullOrEmpty(_headerTag))
                {
                    SizeF tagSz = g.MeasureString(_headerTag, Theme.FontMonoSmall);
                    int tx = Width - (int)Math.Ceiling(tagSz.Width) - 24;
                    Theme.DrawBadge(g, _headerTag, tx, 12, Color.FromArgb(18, 36, 52), Theme.AccentCyan);
                }

                int divY = HeaderHeight - 6;
                using (Pen divPen = new Pen(Color.FromArgb(90, Theme.Border), 1f))
                {
                    g.DrawLine(divPen, 16, divY, Width - 17, divY);
                }
            }
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

        public bool UseMonospace
        {
            get { return _innerBox != null && _innerBox.Font.Name == Theme.FontMono.Name; }
            set
            {
                if (_innerBox != null)
                {
                    _innerBox.Font = value ? Theme.FontMono : Theme.FontRegular;
                }
            }
        }

        public override string Text
        {
            get { return _innerBox.Text; }
            set { _innerBox.Text = value; Invalidate(); }
        }

        public new event EventHandler TextChanged;
        public new event KeyEventHandler KeyDown;
        public new event KeyEventHandler KeyUp;
        public new event KeyPressEventHandler KeyPress;
        public new event EventHandler Leave;

        public new bool Focus()
        {
            return _innerBox != null ? _innerBox.Focus() : base.Focus();
        }

        public void SelectAll()
        {
            if (_innerBox != null) _innerBox.SelectAll();
        }

        public ModernTextBox()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.UserPaint |
                     ControlStyles.OptimizedDoubleBuffer, true);

            this.BackColor = Theme.BgInput;
            this.Height = 34;
            this.Padding = new Padding(10, 8, 10, 7);

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
            _innerBox.Leave += (s, e) => { if (Leave != null) Leave(this, e); };
            _innerBox.TextChanged += (s, e) => { if (TextChanged != null) TextChanged(this, e); Invalidate(); };
            _innerBox.KeyDown += (s, e) => { if (KeyDown != null) KeyDown(this, e); };
            _innerBox.KeyUp += (s, e) => { if (KeyUp != null) KeyUp(this, e); };
            _innerBox.KeyPress += (s, e) => { if (KeyPress != null) KeyPress(this, e); };

            this.Controls.Add(_innerBox);
        }

        protected override void OnPaintBackground(PaintEventArgs e)
        {
            Color outer = (Parent != null) ? Parent.BackColor : Theme.BgCard;
            using (SolidBrush bg = new SolidBrush(outer))
            {
                e.Graphics.FillRectangle(bg, ClientRectangle);
            }
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
                using (Pen pen = new Pen(borderColor, _isFocused ? 1.4f : 1f))
                {
                    g.DrawPath(pen, path);
                }
            }

            if (string.IsNullOrEmpty(_innerBox.Text) && !_isFocused && !string.IsNullOrEmpty(_placeholder))
            {
                using (SolidBrush hintBrush = new SolidBrush(Theme.TextMuted))
                {
                    g.DrawString(_placeholder, _innerBox.Font, hintBrush, 11, (Height - 16) / 2);
                }
            }

            base.OnPaint(e);
        }
    }

    // Custom dark tactical pill toggle switch (replaces white WinForms CheckBox)
    public class TacticalToggle : Control
    {
        private bool _checked = false;
        private bool _hovered = false;

        public bool Checked
        {
            get { return _checked; }
            set
            {
                if (_checked != value)
                {
                    _checked = value;
                    Invalidate();
                    if (CheckedChanged != null) CheckedChanged(this, EventArgs.Empty);
                }
            }
        }

        public Color AccentColor { get; set; }
        public event EventHandler CheckedChanged;

        public TacticalToggle()
        {
            AccentColor = Theme.AccentCyan;
            SetStyle(ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.UserPaint |
                     ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.ResizeRedraw, true);
            this.Height = 26;
            this.Cursor = Cursors.Hand;
            this.Font = Theme.FontRegular;
            this.ForeColor = Theme.TextPrimary;
            this.BackColor = Theme.BgCard;
        }

        protected override void OnMouseEnter(EventArgs e) { _hovered = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { _hovered = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnClick(EventArgs e)
        {
            Checked = !Checked;
            base.OnClick(e);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

            Color bg = (Parent != null) ? Parent.BackColor : Theme.BgCard;
            using (SolidBrush clear = new SolidBrush(bg))
            {
                g.FillRectangle(clear, ClientRectangle);
            }

            int trackW = 38;
            int trackH = 20;
            int trackY = (Height - trackH) / 2;
            Rectangle trackRect = new Rectangle(1, trackY, trackW, trackH);

            Color trackFill = _checked
                ? Color.FromArgb(55, AccentColor)
                : Theme.BgInput;
            Color trackBorder = _checked
                ? AccentColor
                : (_hovered ? Theme.BorderLight : Theme.Border);

            using (GraphicsPath tp = Theme.CreateRoundedPath(trackRect, trackH / 2))
            using (SolidBrush tb = new SolidBrush(trackFill))
            using (Pen tpen = new Pen(trackBorder, 1.2f))
            {
                g.FillPath(tb, tp);
                g.DrawPath(tpen, tp);
            }

            int knobSize = 14;
            int knobX = _checked ? (trackRect.Right - knobSize - 3) : (trackRect.Left + 3);
            int knobY = trackY + (trackH - knobSize) / 2;
            Color knobColor = _checked ? AccentColor : Theme.TextMuted;
            using (SolidBrush kb = new SolidBrush(knobColor))
            {
                g.FillEllipse(kb, knobX, knobY, knobSize, knobSize);
            }

            Color textCol = _checked ? Theme.TextPrimary : (_hovered ? Theme.TextPrimary : Theme.TextSecondary);
            using (SolidBrush txtBrush = new SolidBrush(textCol))
            {
                StringFormat sf = new StringFormat { LineAlignment = StringAlignment.Center };
                g.DrawString(Text, Font, txtBrush, new Rectangle(trackW + 10, 0, Width - trackW - 12, Height), sf);
            }
        }
    }

    // Custom dark monospace port stepper (replaces white WinForms NumericUpDown)
    public class TacticalPortStepper : UserControl
    {
        private TextBox _tbPort;
        private int _value = 27016;
        private int _min = 1024;
        private int _max = 65535;
        private bool _hoverMinus = false;
        private bool _hoverPlus = false;

        public int Minimum { get { return _min; } set { _min = value; } }
        public int Maximum { get { return _max; } set { _max = value; } }

        public int Value
        {
            get { return _value; }
            set
            {
                int clamped = Math.Max(_min, Math.Min(_max, value));
                if (_value != clamped)
                {
                    _value = clamped;
                    if (_tbPort != null && _tbPort.Text != _value.ToString())
                        _tbPort.Text = _value.ToString();
                    Invalidate();
                    if (ValueChanged != null) ValueChanged(this, EventArgs.Empty);
                }
                else if (_tbPort != null && _tbPort.Text != _value.ToString())
                {
                    _tbPort.Text = _value.ToString();
                }
            }
        }

        public event EventHandler ValueChanged;

        public TacticalPortStepper()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.UserPaint |
                     ControlStyles.OptimizedDoubleBuffer, true);

            this.Size = new Size(138, 32);
            this.BackColor = Theme.BgInput;

            _tbPort = new TextBox
            {
                BorderStyle = BorderStyle.None,
                BackColor = Theme.BgInput,
                ForeColor = Theme.AccentCyan,
                Font = Theme.FontMonoBold,
                TextAlign = HorizontalAlignment.Center,
                Text = _value.ToString(),
                Bounds = new Rectangle(30, 8, 78, 18)
            };

            _tbPort.Leave += (s, e) => CommitText();
            _tbPort.KeyDown += (s, e) =>
            {
                if (e.KeyCode == Keys.Enter)
                {
                    CommitText();
                    e.SuppressKeyPress = true;
                }
                else if (e.KeyCode == Keys.Up)
                {
                    Value = _value + 1;
                    e.SuppressKeyPress = true;
                }
                else if (e.KeyCode == Keys.Down)
                {
                    Value = _value - 1;
                    e.SuppressKeyPress = true;
                }
            };

            this.Controls.Add(_tbPort);
        }

        private void CommitText()
        {
            int parsed;
            if (int.TryParse(_tbPort.Text.Trim(), out parsed))
                Value = parsed;
            else
                _tbPort.Text = _value.ToString();
        }

        protected override void OnResize(EventArgs e)
        {
            base.OnResize(e);
            if (_tbPort != null)
            {
                _tbPort.Bounds = new Rectangle(30, (Height - 16) / 2, Math.Max(30, Width - 60), 18);
            }
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            bool hm = e.X < 28;
            bool hp = e.X > Width - 28;
            if (hm != _hoverMinus || hp != _hoverPlus)
            {
                _hoverMinus = hm;
                _hoverPlus = hp;
                Cursor = (hm || hp) ? Cursors.Hand : Cursors.Default;
                Invalidate();
            }
            base.OnMouseMove(e);
        }

        protected override void OnMouseLeave(EventArgs e)
        {
            _hoverMinus = false;
            _hoverPlus = false;
            Cursor = Cursors.Default;
            Invalidate();
            base.OnMouseLeave(e);
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (e.X < 28) Value = _value - 1;
            else if (e.X > Width - 28) Value = _value + 1;
            base.OnMouseDown(e);
        }

        protected override void OnPaintBackground(PaintEventArgs e)
        {
            Color bg = (Parent != null) ? Parent.BackColor : Theme.BgCard;
            using (SolidBrush b = new SolidBrush(bg)) e.Graphics.FillRectangle(b, ClientRectangle);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

            Rectangle rect = new Rectangle(0, 0, Width - 1, Height - 1);
            using (GraphicsPath path = Theme.CreateRoundedPath(rect, 5))
            using (SolidBrush bg = new SolidBrush(Theme.BgInput))
            using (Pen border = new Pen(Theme.BorderLight, 1f))
            {
                g.FillPath(bg, path);
                g.DrawPath(border, path);
            }

            // Left / Right step zones
            if (_hoverMinus)
            {
                using (SolidBrush hb = new SolidBrush(Theme.BgCardHover))
                    g.FillRectangle(hb, 2, 2, 26, Height - 4);
            }
            if (_hoverPlus)
            {
                using (SolidBrush hb = new SolidBrush(Theme.BgCardHover))
                    g.FillRectangle(hb, Width - 28, 2, 26, Height - 4);
            }

            using (Pen div = new Pen(Theme.Border, 1f))
            {
                g.DrawLine(div, 28, 4, 28, Height - 5);
                g.DrawLine(div, Width - 28, 4, Width - 28, Height - 5);
            }

            using (SolidBrush mBrush = new SolidBrush(_hoverMinus ? Theme.AccentCyan : Theme.TextSecondary))
            using (SolidBrush pBrush = new SolidBrush(_hoverPlus ? Theme.AccentCyan : Theme.TextSecondary))
            {
                StringFormat sf = new StringFormat { Alignment = StringAlignment.Center, LineAlignment = StringAlignment.Center };
                g.DrawString("-", Theme.FontMonoBold, mBrush, new Rectangle(0, 0, 28, Height), sf);
                g.DrawString("+", Theme.FontMonoBold, pBrush, new Rectangle(Width - 28, 0, 28, Height), sf);
            }
        }
    }

    // Owner-drawn rounded status pill badge with glowing dot
    public class StatusPillBadge : Control
    {
        private Color _accentColor = Theme.AccentGreen;
        private Color _fillColor = Color.FromArgb(16, 44, 34);

        public Color AccentColor
        {
            get { return _accentColor; }
            set { _accentColor = value; Invalidate(); }
        }

        public Color FillColor
        {
            get { return _fillColor; }
            set { _fillColor = value; Invalidate(); }
        }

        public override string Text
        {
            get { return base.Text; }
            set { base.Text = value; AdjustSize(); Invalidate(); }
        }

        public StatusPillBadge()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.UserPaint |
                     ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.ResizeRedraw, true);
            this.Font = Theme.FontBadge;
            this.Height = 24;
            this.BackColor = Theme.BgDarkest;
        }

        private void AdjustSize()
        {
            try
            {
                using (Graphics g = CreateGraphics())
                {
                    SizeF sz = g.MeasureString(Text ?? "", Font);
                    this.Width = Math.Max(60, (int)Math.Ceiling(sz.Width) + 28);
                }
            }
            catch { }
        }

        protected override void OnPaintBackground(PaintEventArgs e)
        {
            Color outer = (Parent != null) ? Parent.BackColor : Theme.BgDarkest;
            using (SolidBrush b = new SolidBrush(outer)) e.Graphics.FillRectangle(b, ClientRectangle);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

            Rectangle rect = new Rectangle(0, 0, Width - 1, Height - 1);
            using (GraphicsPath path = Theme.CreateRoundedPath(rect, 11))
            using (SolidBrush fill = new SolidBrush(_fillColor))
            using (Pen border = new Pen(Color.FromArgb(95, _accentColor), 1f))
            {
                g.FillPath(fill, path);
                g.DrawPath(border, path);
            }

            // Glowing status dot
            using (SolidBrush glow = new SolidBrush(Color.FromArgb(70, _accentColor)))
            using (SolidBrush dot = new SolidBrush(_accentColor))
            {
                g.FillEllipse(glow, 7, (Height - 10) / 2, 10, 10);
                g.FillEllipse(dot, 9, (Height - 6) / 2, 6, 6);
            }

            using (SolidBrush textBrush = new SolidBrush(_accentColor))
            {
                StringFormat sf = new StringFormat { LineAlignment = StringAlignment.Center };
                g.DrawString(Text, Font, textBrush, new Rectangle(20, 0, Width - 24, Height), sf);
            }
        }
    }

    // Animated Clear Sky PDA Signal Radar widget for Lobby discovery
    public class TacticalRadarPanel : Control
    {
        private float _sweepAngle = 315f;
        private Timer _animTimer;
        private int _beaconCount = 0;
        private string _statusLine = "SCANNING LAN...";
        private readonly List<PointF> _blips = new List<PointF>();

        public int BeaconCount
        {
            get { return _beaconCount; }
            set
            {
                _beaconCount = value;
                _statusLine = value > 0
                    ? string.Format("{0} SQUAD BEACON{1} ACTIVE", value, value == 1 ? "" : "S")
                    : "SCANNING UDP 27015...";
                RebuildBlips();
                Invalidate();
            }
        }

        public TacticalRadarPanel()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint |
                     ControlStyles.UserPaint |
                     ControlStyles.OptimizedDoubleBuffer |
                     ControlStyles.ResizeRedraw, true);

            this.BackColor = Theme.BgCard;
            _animTimer = new Timer { Interval = 55 };
            _animTimer.Tick += (s, e) =>
            {
                if (!Visible) return;
                _sweepAngle = (_sweepAngle + 3.5f) % 360f;
                Invalidate();
            };
            _animTimer.Start();
        }

        private void RebuildBlips()
        {
            _blips.Clear();
            for (int i = 0; i < Math.Min(_beaconCount, 6); i++)
            {
                double ang = (i * 1.15 + 0.65);
                double rad = 0.35 + ((i * 19) % 45) / 100.0;
                _blips.Add(new PointF((float)(Math.Cos(ang) * rad), (float)(Math.Sin(ang) * rad)));
            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing && _animTimer != null)
            {
                _animTimer.Stop();
                _animTimer.Dispose();
                _animTimer = null;
            }
            base.Dispose(disposing);
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            Graphics g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;

            Color bg = (Parent != null) ? Parent.BackColor : Theme.BgCard;
            using (SolidBrush clear = new SolidBrush(bg))
            {
                g.FillRectangle(clear, ClientRectangle);
            }

            int padBottom = 38;
            int size = Math.Max(40, Math.Min(Width - 16, Height - padBottom - 8));
            int cx = Width / 2;
            int cy = (Height - padBottom) / 2 + 2;
            int r = size / 2;

            Rectangle circleRect = new Rectangle(cx - r, cy - r, r * 2, r * 2);

            // Recessed radar screen circle
            using (SolidBrush screenBrush = new SolidBrush(Theme.BgInput))
            {
                g.FillEllipse(screenBrush, circleRect);
            }

            // Sweeping sector wedge (40 degrees trail)
            using (GraphicsPath wedge = new GraphicsPath())
            {
                wedge.AddPie(circleRect, _sweepAngle - 42f, 42f);
                using (SolidBrush sweepBrush = new SolidBrush(Color.FromArgb(42, Theme.AccentCyan)))
                {
                    g.FillPath(sweepBrush, wedge);
                }
            }

            // Concentric tactical rings & crosshair
            using (Pen ringPen = new Pen(Color.FromArgb(65, Theme.AccentCyan), 1f))
            using (Pen outerPen = new Pen(Color.FromArgb(140, Theme.AccentCyan), 1.3f))
            {
                g.DrawEllipse(outerPen, circleRect);
                int r2 = (int)(r * 0.66f);
                int r1 = (int)(r * 0.33f);
                g.DrawEllipse(ringPen, cx - r2, cy - r2, r2 * 2, r2 * 2);
                g.DrawEllipse(ringPen, cx - r1, cy - r1, r1 * 2, r1 * 2);
                g.DrawLine(ringPen, cx - r, cy, cx + r, cy);
                g.DrawLine(ringPen, cx, cy - r, cx, cy + r);
            }

            // Leading sweep beam
            double radAngle = _sweepAngle * Math.PI / 180.0;
            int bx = cx + (int)Math.Round(r * Math.Cos(radAngle));
            int by = cy + (int)Math.Round(r * Math.Sin(radAngle));
            using (Pen beamPen = new Pen(Color.FromArgb(200, Theme.AccentCyan), 1.6f))
            {
                g.DrawLine(beamPen, cx, cy, bx, by);
            }

            // Center emitter dot
            using (SolidBrush centerBrush = new SolidBrush(Theme.AccentCyan))
            {
                g.FillEllipse(centerBrush, cx - 3, cy - 3, 6, 6);
            }

            // Active server blips
            foreach (PointF b in _blips)
            {
                int px = cx + (int)(b.X * r);
                int py = cy + (int)(b.Y * r);
                using (SolidBrush halo = new SolidBrush(Color.FromArgb(80, Theme.AccentGreen)))
                using (SolidBrush dot = new SolidBrush(Theme.AccentGreen))
                {
                    g.FillEllipse(halo, px - 6, py - 6, 12, 12);
                    g.FillEllipse(dot, px - 3, py - 3, 6, 6);
                }
            }

            // Status readout below radar
            Color statusCol = _beaconCount > 0 ? Theme.AccentGreen : Theme.AccentCyan;
            using (SolidBrush sb = new SolidBrush(statusCol))
            {
                StringFormat sf = new StringFormat { Alignment = StringAlignment.Center };
                g.DrawString(_statusLine, Theme.FontMonoSmall, sb, new Rectangle(4, Height - 32, Width - 8, 16), sf);
            }
            using (SolidBrush mb = new SolidBrush(Theme.TextMuted))
            {
                StringFormat sf = new StringFormat { Alignment = StringAlignment.Center };
                g.DrawString("UDP 27015 // [F5] SCAN", Theme.FontMonoSmall, mb, new Rectangle(4, Height - 16, Width - 8, 14), sf);
            }
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
            this.Height = 38;
            this.BackColor = Theme.BgSidebar;

            _lblClose = new Label
            {
                Text = "✕",
                Font = new Font("Segoe UI", 9.5f, FontStyle.Regular),
                ForeColor = Theme.TextSecondary,
                Size = new Size(46, 38),
                TextAlign = ContentAlignment.MiddleCenter,
                Dock = DockStyle.Right,
                Cursor = Cursors.Hand
            };
            _lblClose.MouseEnter += (s, e) => { _lblClose.BackColor = Theme.AccentRed; _lblClose.ForeColor = Color.White; };
            _lblClose.MouseLeave += (s, e) => { _lblClose.BackColor = Theme.BgSidebar; _lblClose.ForeColor = Theme.TextSecondary; };
            _lblClose.Click += (s, e) => _parentForm.Close();

            _lblMinimize = new Label
            {
                Text = "—",
                Font = new Font("Segoe UI", 9.5f, FontStyle.Regular),
                ForeColor = Theme.TextSecondary,
                Size = new Size(46, 38),
                TextAlign = ContentAlignment.MiddleCenter,
                Dock = DockStyle.Right,
                Cursor = Cursors.Hand
            };
            _lblMinimize.MouseEnter += (s, e) => { _lblMinimize.BackColor = Theme.BgCardHover; _lblMinimize.ForeColor = Color.White; };
            _lblMinimize.MouseLeave += (s, e) => { _lblMinimize.BackColor = Theme.BgSidebar; _lblMinimize.ForeColor = Theme.TextSecondary; };
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

            Theme.DrawFactionLogo(g, new Rectangle(14, 8, 22, 22));

            int titleWidth = 0;
            using (SolidBrush textBrush = new SolidBrush(Theme.TextPrimary))
            {
                g.DrawString("S.T.A.L.K.E.R. CLEAR SKY", Theme.FontBadge, textBrush, 44, 12);
                SizeF size = g.MeasureString("S.T.A.L.K.E.R. CLEAR SKY", Theme.FontBadge);
                titleWidth = (int)Math.Ceiling(size.Width);
            }

            int badgeX = 44 + titleWidth + 12;
            Theme.DrawBadge(g, "PDA CO-OP v1.4", badgeX, 9, Color.FromArgb(16, 44, 68), Theme.AccentCyan);

            using (SolidBrush subBrush = new SolidBrush(Theme.TextMuted))
            {
                g.DrawString("TACTICAL SQUAD UPLINK", Theme.FontMonoSmall, subBrush, badgeX + 110, 12);
            }

            using (Pen borderPen = new Pen(Theme.Border, 1f))
            {
                g.DrawLine(borderPen, 0, Height - 1, Width, Height - 1);
            }

            base.OnPaint(e);
        }
    }
}
