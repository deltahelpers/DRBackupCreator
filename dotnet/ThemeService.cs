using System.Drawing;
using System.Windows.Forms;

namespace DRBackupCreator
{
    internal sealed class ThemePalette
    {
        public Color Window;
        public Color Surface;
        public Color Text;
        public Color Muted;
        public Color Border;
        public Color Accent;

        public static ThemePalette For(string mode)
        {
            if (mode == "light")
                return new ThemePalette
                {
                    Window = Color.FromArgb(245, 247, 250),
                    Surface = Color.White,
                    Text = Color.FromArgb(32, 38, 46),
                    Muted = Color.FromArgb(98, 107, 118),
                    Border = Color.FromArgb(218, 223, 230),
                    Accent = Color.FromArgb(48, 105, 190)
                };
            return new ThemePalette
            {
                Window = Color.FromArgb(30, 32, 36),
                Surface = Color.FromArgb(39, 42, 48),
                Text = Color.FromArgb(235, 238, 242),
                Muted = Color.FromArgb(164, 171, 181),
                Border = Color.FromArgb(67, 72, 81),
                Accent = Color.FromArgb(105, 157, 245)
            };
        }
    }

    internal static class ThemeService
    {
        public static void Apply(Control control, string mode)
        {
            ThemePalette palette = ThemePalette.For(mode);
            ApplyTo(control, palette);
        }

        private static void ApplyTo(Control control, ThemePalette palette)
        {
            bool surface = control is Button || control is TextBox || control is ListView ||
                           control is ComboBox || control is CheckBox || control is DateTimePicker;
            control.BackColor = surface ? palette.Surface : palette.Window;
            control.ForeColor = palette.Text;

            var button = control as Button;
            if (button != null)
            {
                button.FlatStyle = FlatStyle.Flat;
                button.FlatAppearance.BorderColor = palette.Border;
                button.FlatAppearance.MouseOverBackColor = palette.Border;
                button.FlatAppearance.MouseDownBackColor = palette.Accent;
            }

            var list = control as ListView;
            if (list != null)
            {
                list.BorderStyle = BorderStyle.FixedSingle;
                list.GridLines = true;
            }

            var menu = control as ToolStrip;
            if (menu != null)
            {
                menu.BackColor = palette.Surface;
                menu.ForeColor = palette.Text;
                foreach (ToolStripItem item in menu.Items)
                    ApplyTo(item, palette);
            }

            foreach (Control child in control.Controls) ApplyTo(child, palette);
        }

        private static void ApplyTo(ToolStripItem item, ThemePalette palette)
        {
            item.BackColor = palette.Surface;
            item.ForeColor = palette.Text;
            var dropDown = item as ToolStripDropDownItem;
            if (dropDown != null)
                foreach (ToolStripItem child in dropDown.DropDownItems)
                    ApplyTo(child, palette);
        }
    }

    internal static class UiText
    {
        public static string Get(AppSettings settings, string english, string spanish)
        {
            return settings.Language == "es" ? spanish : english;
        }
    }
}
