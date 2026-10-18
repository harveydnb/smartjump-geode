using System.Diagnostics;
using System.Drawing;
using System.Windows.Forms;

namespace SmartJumpLauncher;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new LauncherForm());
    }
}

internal sealed class LauncherForm : Form
{
    private readonly Label status;
    private readonly System.Windows.Forms.Timer timer = new() { Interval = 2000 };

    public LauncherForm()
    {
        Text = "Smart Jump Launcher — Preview";
        ClientSize = new Size(470, 260);
        MinimumSize = new Size(470, 260);
        StartPosition = FormStartPosition.CenterScreen;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;

        var heading = new Label
        {
            Text = "Smart Jump Launcher (preview)",
            Font = new Font("Segoe UI", 16, FontStyle.Bold),
            AutoSize = true,
            Location = new Point(20, 18)
        };
        Controls.Add(heading);

        status = new Label
        {
            Text = "Checking for Geometry Dash...",
            Font = new Font("Segoe UI", 11),
            AutoSize = true,
            Location = new Point(20, 66)
        };
        Controls.Add(status);

        var info = new Label
        {
            Text = "Target game version: 2.2081 (not automatically verified)\n" +
                   "Smart Jump: NOT IMPLEMENTED\n" +
                   "99.5% hitbox scaling: NOT IMPLEMENTED\n\n" +
                   "This preview only detects the running game.\n" +
                   "It does not modify Geometry Dash or inject code.",
            Font = new Font("Segoe UI", 10),
            AutoSize = true,
            Location = new Point(20, 105)
        };
        Controls.Add(info);

        timer.Tick += (_, _) => UpdateStatus();
        timer.Start();
        UpdateStatus();
    }

    private void UpdateStatus()
    {
        try
        {
            bool running = Process.GetProcessesByName("GeometryDash").Length > 0;
            status.Text = running
                ? "Geometry Dash: RUNNING"
                : "Geometry Dash: NOT DETECTED";
            status.ForeColor = running ? Color.DarkGreen : Color.DarkOrange;
        }
        catch (Exception)
        {
            status.Text = "Geometry Dash: Unable to check process";
            status.ForeColor = Color.DarkRed;
        }
    }

    protected override void OnFormClosed(FormClosedEventArgs e)
    {
        timer.Dispose();
        base.OnFormClosed(e);
    }
}
