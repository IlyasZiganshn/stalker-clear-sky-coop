using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace ClearSkyCoop.Launcher
{
    public static class Theme
    {
        // Core Palette - Tactical Clear Sky Dark Aesthetic
        public static readonly Color BgDarkest       = Color.FromArgb(14, 16, 20);     // #0E1014
        public static readonly Color BgSidebar       = Color.FromArgb(20, 23, 29);     // #14171D
        public static readonly Color BgCard          = Color.FromArgb(27, 31, 38);     // #1B1F26
        public static readonly Color BgCardHover     = Color.FromArgb(34, 39, 48);     // #222730
        public static readonly Color BgInput         = Color.FromArgb(18, 21, 26);     // #12151A
        public static readonly Color Border          = Color.FromArgb(42, 48, 59);     // #2A303B
        public static readonly Color BorderLight     = Color.FromArgb(58, 66, 80);     // #3A4250
        public static readonly Color BorderFocus     = Color.FromArgb(0, 163, 255);    // #00A3FF (Clear Sky Cyan)
        
        // Accents
        public static readonly Color AccentCyan      = Color.FromArgb(0, 163, 255);    // #00A3FF
        public static readonly Color AccentCyanHover = Color.FromArgb(38, 178, 255);   // #26B2FF
        public static readonly Color AccentGreen     = Color.FromArgb(16, 185, 129);   // #10B981 (Emerald)
        public static readonly Color AccentGreenHover= Color.FromArgb(5, 150, 105);    // #059669
        public static readonly Color AccentAmber     = Color.FromArgb(245, 158, 11);   // #F59E0B
        public static readonly Color AccentRed       = Color.FromArgb(239, 68, 68);    // #EF4444
        public static readonly Color AccentRedHover  = Color.FromArgb(220, 38, 38);

        // Typography
        public static readonly Color TextPrimary     = Color.FromArgb(245, 247, 250);  // Crisp white
        public static readonly Color TextSecondary   = Color.FromArgb(156, 163, 175);  // Slate silver
        public static readonly Color TextMuted       = Color.FromArgb(107, 114, 128);  // Hints / tags
        
        // Fonts
        public static readonly Font FontTitle        = new Font("Segoe UI", 12f, FontStyle.Bold);
        public static readonly Font FontSubtitle     = new Font("Segoe UI", 10f, FontStyle.Bold);
        public static readonly Font FontRegular      = new Font("Segoe UI", 9.5f, FontStyle.Regular);
        public static readonly Font FontRegularBold  = new Font("Segoe UI", 9.5f, FontStyle.Bold);
        public static readonly Font FontSmall        = new Font("Segoe UI", 8.5f, FontStyle.Regular);
        public static readonly Font FontBadge        = new Font("Segoe UI", 8.0f, FontStyle.Bold);
        public static readonly Font FontHeroBtn      = new Font("Segoe UI", 11.5f, FontStyle.Bold);

        // Graphics Utility Helpers
        public static GraphicsPath CreateRoundedPath(Rectangle rect, int radius)
        {
            GraphicsPath path = new GraphicsPath();
            if (radius <= 0)
            {
                path.AddRectangle(rect);
                return path;
            }

            int diameter = radius * 2;
            Rectangle arc = new Rectangle(rect.X, rect.Y, diameter, diameter);

            // Top-left
            path.AddArc(arc, 180, 90);
            // Top-right
            arc.X = rect.Right - diameter;
            path.AddArc(arc, 270, 90);
            // Bottom-right
            arc.Y = rect.Bottom - diameter;
            path.AddArc(arc, 0, 90);
            // Bottom-left
            arc.X = rect.Left;
            path.AddArc(arc, 90, 90);

            path.CloseFigure();
            return path;
        }

        public static void DrawBadge(Graphics g, string text, int x, int y, Color bgColor, Color textColor)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            SizeF size = g.MeasureString(text, FontBadge);
            int padH = 7;
            int padV = 3;
            Rectangle rect = new Rectangle(x, y, (int)size.Width + padH * 2, (int)size.Height + padV * 2);

            using (GraphicsPath path = CreateRoundedPath(rect, 4))
            using (SolidBrush brush = new SolidBrush(bgColor))
            using (SolidBrush textBrush = new SolidBrush(textColor))
            {
                g.FillPath(brush, path);
                g.DrawString(text, FontBadge, textBrush, x + padH, y + padV);
            }
        }
    }
}
