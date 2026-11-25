using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace ClearSkyCoop.Launcher
{
    public enum IconType
    {
        None,
        Radar,
        Bolt,
        Gear,
        Wrench,
        Play,
        Refresh,
        Folder,
        Copy,
        User,
        Shield,
        Signal
    }

    public static class Theme
    {
        // Clear Sky Tactical PDA (2026 Tonal Dark Architecture)
        public static readonly Color BgDarkest       = Color.FromArgb(10, 14, 20);     // #0A0E14 - PDA Chassis Deep Obsidian
        public static readonly Color BgSidebar       = Color.FromArgb(14, 19, 28);     // #0E131C - Navigation Rail
        public static readonly Color BgCard          = Color.FromArgb(20, 27, 38);     // #141B26 - Tactical Surface
        public static readonly Color BgCardElevated  = Color.FromArgb(26, 35, 50);     // #1A2332 - Elevated Module / Radar Screen
        public static readonly Color BgCardHover     = Color.FromArgb(32, 43, 60);     // #202B3C - Interactive Hover
        public static readonly Color BgInput         = Color.FromArgb(12, 16, 23);     // #0C1017 - Recessed Field
        public static readonly Color Border          = Color.FromArgb(35, 48, 68);     // #233044 - Tactical Hairline
        public static readonly Color BorderLight     = Color.FromArgb(52, 71, 98);     // #344762 - Highlight Border
        public static readonly Color BorderFocus     = Color.FromArgb(0, 180, 255);    // #00B4FF - Clear Sky Signal Focus

        // Faction & Telemetry Accents
        public static readonly Color AccentCyan      = Color.FromArgb(0, 180, 255);    // #00B4FF - Clear Sky Beacon
        public static readonly Color AccentCyanDark  = Color.FromArgb(0, 118, 204);    // #0076CC
        public static readonly Color AccentCyanHover = Color.FromArgb(48, 200, 255);   // #30C8FF
        public static readonly Color AccentGreen     = Color.FromArgb(16, 212, 137);   // #10D489 - Online / Ready
        public static readonly Color AccentGreenDark = Color.FromArgb(7, 146, 102);    // #079266
        public static readonly Color AccentGreenHover= Color.FromArgb(36, 232, 156);
        public static readonly Color AccentAmber     = Color.FromArgb(245, 158, 11);   // #F59E0B - Stalker Amber Warning
        public static readonly Color AccentRed       = Color.FromArgb(244, 63, 94);    // #F43F5E - Danger / Offline
        public static readonly Color AccentRedHover  = Color.FromArgb(225, 29, 72);

        // Typography Colors (High Contrast)
        public static readonly Color TextPrimary     = Color.FromArgb(244, 247, 252);  // #F4F7FC
        public static readonly Color TextSecondary   = Color.FromArgb(162, 178, 198);  // #A2B2C6
        public static readonly Color TextMuted       = Color.FromArgb(108, 125, 148);  // #6C7D94

        // Fonts (UI + Monospace Telemetry)
        public static readonly Font FontAppTitle     = new Font("Segoe UI", 12.0f, FontStyle.Bold);
        public static readonly Font FontTitle        = new Font("Segoe UI", 11.5f, FontStyle.Bold);
        public static readonly Font FontSubtitle     = new Font("Segoe UI", 9.5f, FontStyle.Bold);
        public static readonly Font FontRegular      = new Font("Segoe UI", 9.25f, FontStyle.Regular);
        public static readonly Font FontRegularBold  = new Font("Segoe UI", 9.25f, FontStyle.Bold);
        public static readonly Font FontSmall        = new Font("Segoe UI", 8.25f, FontStyle.Regular);
        public static readonly Font FontSmallBold    = new Font("Segoe UI", 8.25f, FontStyle.Bold);
        public static readonly Font FontBadge        = new Font("Segoe UI", 7.75f, FontStyle.Bold);
        public static readonly Font FontHeroBtn      = new Font("Segoe UI", 10.5f, FontStyle.Bold);
        public static readonly Font FontMono         = new Font("Consolas", 9.5f, FontStyle.Regular);
        public static readonly Font FontMonoBold     = new Font("Consolas", 9.5f, FontStyle.Bold);
        public static readonly Font FontMonoSmall    = new Font("Consolas", 8.25f, FontStyle.Regular);

        public static GraphicsPath CreateRoundedPath(Rectangle rect, int radius)
        {
            GraphicsPath path = new GraphicsPath();
            if (radius <= 0)
            {
                path.AddRectangle(rect);
                return path;
            }

            int diameter = Math.Min(radius * 2, Math.Min(rect.Width, rect.Height));
            if (diameter <= 1)
            {
                path.AddRectangle(rect);
                return path;
            }

            Rectangle arc = new Rectangle(rect.X, rect.Y, diameter, diameter);
            path.AddArc(arc, 180, 90);
            arc.X = rect.Right - diameter;
            path.AddArc(arc, 270, 90);
            arc.Y = rect.Bottom - diameter;
            path.AddArc(arc, 0, 90);
            arc.X = rect.Left;
            path.AddArc(arc, 90, 90);
            path.CloseFigure();
            return path;
        }

        // Subtle PDA corner reticles for tactical hardware aesthetic
        public static void DrawCornerReticles(Graphics g, Rectangle rect, Color color, int len = 7)
        {
            using (Pen pen = new Pen(color, 1.3f))
            {
                // Top-left
                g.DrawLine(pen, rect.Left, rect.Top + len, rect.Left, rect.Top);
                g.DrawLine(pen, rect.Left, rect.Top, rect.Left + len, rect.Top);
                // Top-right
                g.DrawLine(pen, rect.Right - len, rect.Top, rect.Right, rect.Top);
                g.DrawLine(pen, rect.Right, rect.Top, rect.Right, rect.Top + len);
                // Bottom-left
                g.DrawLine(pen, rect.Left, rect.Bottom - len, rect.Left, rect.Bottom);
                g.DrawLine(pen, rect.Left, rect.Bottom, rect.Left + len, rect.Bottom);
                // Bottom-right
                g.DrawLine(pen, rect.Right - len, rect.Bottom, rect.Right, rect.Bottom);
                g.DrawLine(pen, rect.Right, rect.Bottom - len, rect.Right, rect.Bottom);
            }
        }

        public static void DrawBadge(Graphics g, string text, int x, int y, Color bgColor, Color textColor)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            SizeF size = g.MeasureString(text, FontBadge);
            int padH = 8;
            int padV = 3;
            Rectangle rect = new Rectangle(x, y, (int)Math.Ceiling(size.Width) + padH * 2, (int)Math.Ceiling(size.Height) + padV * 2);

            using (GraphicsPath path = CreateRoundedPath(rect, 5))
            using (SolidBrush brush = new SolidBrush(bgColor))
            using (Pen borderPen = new Pen(Color.FromArgb(70, textColor), 1f))
            using (SolidBrush textBrush = new SolidBrush(textColor))
            {
                g.FillPath(brush, path);
                g.DrawPath(borderPen, path);
                g.DrawString(text, FontBadge, textBrush, x + padH, y + padV);
            }
        }

        public static void DrawVectorIcon(Graphics g, IconType icon, Rectangle rect, Color color)
        {
            if (icon == IconType.None) return;
            g.SmoothingMode = SmoothingMode.AntiAlias;

            int cx = rect.X + rect.Width / 2;
            int cy = rect.Y + rect.Height / 2;

            using (Pen pen = new Pen(color, 1.7f))
            using (SolidBrush brush = new SolidBrush(color))
            {
                switch (icon)
                {
                    case IconType.Radar:
                        g.DrawEllipse(pen, cx - 7, cy - 7, 14, 14);
                        g.DrawEllipse(pen, cx - 3, cy - 3, 6, 6);
                        g.DrawLine(pen, cx, cy - 7, cx, cy - 9);
                        g.DrawLine(pen, cx, cy + 7, cx, cy + 9);
                        g.DrawLine(pen, cx - 7, cy, cx - 9, cy);
                        g.DrawLine(pen, cx + 7, cy, cx + 9, cy);
                        break;

                    case IconType.Bolt:
                        Point[] bolt = new Point[] {
                            new Point(cx + 1, cy - 8),
                            new Point(cx - 5, cy),
                            new Point(cx - 1, cy),
                            new Point(cx - 3, cy + 8),
                            new Point(cx + 5, cy - 1),
                            new Point(cx + 1, cy - 1)
                        };
                        g.FillPolygon(brush, bolt);
                        break;

                    case IconType.Gear:
                        g.DrawEllipse(pen, cx - 5, cy - 5, 10, 10);
                        g.FillEllipse(brush, cx - 2, cy - 2, 4, 4);
                        for (int a = 0; a < 360; a += 45)
                        {
                            double rad = a * Math.PI / 180.0;
                            int x1 = cx + (int)Math.Round(5.5 * Math.Cos(rad));
                            int y1 = cy + (int)Math.Round(5.5 * Math.Sin(rad));
                            int x2 = cx + (int)Math.Round(7.8 * Math.Cos(rad));
                            int y2 = cy + (int)Math.Round(7.8 * Math.Sin(rad));
                            g.DrawLine(pen, x1, y1, x2, y2);
                        }
                        break;

                    case IconType.Wrench:
                        g.DrawLine(pen, cx - 6, cy + 6, cx + 6, cy - 6);
                        g.DrawLine(pen, cx - 6, cy - 6, cx + 6, cy + 6);
                        g.DrawEllipse(pen, cx + 3, cy - 8, 4, 4);
                        g.DrawEllipse(pen, cx - 7, cy - 8, 4, 4);
                        break;

                    case IconType.Play:
                        Point[] arrow = new Point[] {
                            new Point(cx - 4, cy - 6),
                            new Point(cx + 6, cy),
                            new Point(cx - 4, cy + 6)
                        };
                        g.FillPolygon(brush, arrow);
                        break;

                    case IconType.Refresh:
                        g.DrawArc(pen, cx - 6, cy - 6, 12, 12, 45, 270);
                        Point[] rHead = new Point[] {
                            new Point(cx + 3, cy - 8),
                            new Point(cx + 7, cy - 3),
                            new Point(cx + 1, cy - 3)
                        };
                        g.FillPolygon(brush, rHead);
                        break;

                    case IconType.Folder:
                        g.DrawRectangle(pen, cx - 7, cy - 4, 14, 10);
                        g.DrawLine(pen, cx - 7, cy - 4, cx - 3, cy - 7);
                        g.DrawLine(pen, cx - 3, cy - 7, cx + 1, cy - 7);
                        g.DrawLine(pen, cx + 1, cy - 7, cx + 3, cy - 4);
                        break;

                    case IconType.Copy:
                        g.DrawRectangle(pen, cx - 6, cy - 6, 8, 9);
                        g.DrawRectangle(pen, cx - 2, cy - 2, 8, 9);
                        break;

                    case IconType.User:
                        g.DrawEllipse(pen, cx - 3, cy - 7, 6, 6);
                        g.DrawArc(pen, cx - 7, cy + 1, 14, 10, 190, 160);
                        break;

                    case IconType.Shield:
                        Point[] shield = new Point[] {
                            new Point(cx, cy - 7),
                            new Point(cx + 6, cy - 4),
                            new Point(cx + 5, cy + 3),
                            new Point(cx, cy + 7),
                            new Point(cx - 5, cy + 3),
                            new Point(cx - 6, cy - 4)
                        };
                        g.DrawPolygon(pen, shield);
                        break;

                    case IconType.Signal:
                        g.FillEllipse(brush, cx - 2, cy + 3, 4, 4);
                        g.DrawArc(pen, cx - 5, cy - 1, 10, 10, 225, 90);
                        g.DrawArc(pen, cx - 8, cy - 5, 16, 16, 225, 90);
                        break;
                }
            }
        }

        public static void DrawFactionLogo(Graphics g, Rectangle bounds)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            int cx = bounds.X + bounds.Width / 2;
            int cy = bounds.Y + bounds.Height / 2;
            int r = Math.Min(bounds.Width, bounds.Height) / 2 - 2;

            using (Pen glowPen = new Pen(Color.FromArgb(45, AccentCyan), 3f))
            {
                g.DrawEllipse(glowPen, cx - r, cy - r, r * 2, r * 2);
            }
            using (Pen outerPen = new Pen(AccentCyan, 1.5f))
            {
                g.DrawEllipse(outerPen, cx - r, cy - r, r * 2, r * 2);
            }

            int innerR = (int)(r * 0.56f);
            using (Pen innerPen = new Pen(Color.FromArgb(120, AccentCyan), 1f))
            {
                innerPen.DashStyle = DashStyle.Dash;
                g.DrawEllipse(innerPen, cx - innerR, cy - innerR, innerR * 2, innerR * 2);
            }

            using (Pen tickPen = new Pen(AccentCyan, 1.5f))
            {
                g.DrawLine(tickPen, cx, cy - r, cx, cy - r + 5);
                g.DrawLine(tickPen, cx, cy + r - 5, cx, cy + r);
                g.DrawLine(tickPen, cx - r, cy, cx - r + 5, cy);
                g.DrawLine(tickPen, cx + r - 5, cy, cx + r, cy);
            }

            using (SolidBrush coreBrush = new SolidBrush(Color.White))
            {
                g.FillEllipse(coreBrush, cx - 2, cy - 2, 4, 4);
            }
        }
    }
}
