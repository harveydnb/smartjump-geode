using System.Diagnostics;
using System.Drawing;
using System.Text.Json;
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

internal sealed class LauncherSettings
{
    public bool SmartJumpRequested { get; set; } = true;
    public bool HitboxScalingRequested { get; set; } = true;
    public double HitboxScale { get; set; } = 99.5;
}

internal sealed class LauncherForm : Form
{
    private readonly Label status = new();
    private readonly Label saveStatus = new();
    private readonly CheckBox jumpPreference = new();
    private readonly CheckBox hitboxPreference = new();
    private readonly System.Windows.Forms.Timer timer = new() { Interval = 2000 };
    private readonly string settingsPath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "SmartJumpLauncher", "settings.json");

    public LauncherForm()
    {
        Text = "Smart Jump Launcher — Configuration Preview";
        ClientSize = new Size(520, 390);
        MinimumSize = new Size(520, 390);
        StartPosition = FormStartPosition.CenterScreen;
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;

        Controls.Add(new Label
        {
            Text = "Smart Jump Launcher",
            Font = new Font("Segoe UI", 17, FontStyle.Bold),
            AutoSize = true,
            Location = new Point(20, 16)
        });

        status.Font = new Font("Segoe UI", 11);
        status.AutoSize = true;
        status.Location = new Point(20, 64);
        Controls.Add(status);

        Controls.Add(new Label
        {
            Text = "Target: Geometry Dash 2.2081 (version not verified)",
            AutoSize = true,
            Location = new Point(20, 98)
        });

        var group = new GroupBox
        {
            Text = "Saved preferences — NOT active in game",
            Location = new Point(20, 126),
            Size = new Size(480, 130)
        };
        Controls.Add(group);

        jumpPreference.Text = "Request Smart Jump (all seven modes)";
        jumpPreference.AutoSize = true;
        jumpPreference.Location = new Point(15, 30);
        group.Controls.Add(jumpPreference);

        hitboxPreference.Text = "Request 99.5% spike/wall hitbox scale";
        hitboxPreference.AutoSize = true;
        hitboxPreference.Location = new Point(15, 66);
        group.Controls.Add(hitboxPreference);

        var save = new Button
        {
            Text = "Save preferences",
            Location = new Point(20, 269),
            Size = new Size(145, 32)
        };
        save.Click += (_, _) => SaveSettings();
        Controls.Add(save);

        saveStatus.AutoSize = true;
        saveStatus.Location = new Point(178, 277);
        Controls.Add(saveStatus);

        Controls.Add(new Label
        {
            Text = "Game modifications are NOT implemented. These options only save\n" +
                   "your choices for a future game-integration component.",
            ForeColor = Color.DarkRed,
            AutoSize = true,
            Location = new Point(20, 323)
        });

        LoadSettings();
        timer.Tick += (_, _) => UpdateStatus();
        timer.Start();
        UpdateStatus();
    }

    private void LoadSettings()
    {
        try
        {
            if (!File.Exists(settingsPath))
            {
                jumpPreference.Checked = true;
                hitboxPreference.Checked = true;
                return;
            }
            var settings = JsonSerializer.Deserialize<LauncherSettings>(File.ReadAllText(settingsPath))
                           ?? new LauncherSettings();
            jumpPreference.Checked = settings.SmartJumpRequested;
            hitboxPreference.Checked = settings.HitboxScalingRequested;
        }
        catch (Exception ex)
        {
            saveStatus.Text = "Could not read settings: " + ex.Message;
        }
    }

    private void SaveSettings()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(settingsPath)!);
            var settings = new LauncherSettings
            {
                SmartJumpRequested = jumpPreference.Checked,
                HitboxScalingRequested = hitboxPreference.Checked,
                HitboxScale = 99.5
            };
            File.WriteAllText(settingsPath,
                JsonSerializer.Serialize(settings, new JsonSerializerOptions { WriteIndented = true }));
            saveStatus.Text = "Preferences saved";
            saveStatus.ForeColor = Color.DarkGreen;
        }
        catch (Exception ex)
        {
            saveStatus.Text = "Save failed: " + ex.Message;
            saveStatus.ForeColor = Color.DarkRed;
        }
    }

    private void UpdateStatus()
    {
        try
        {
            using var process = Process.GetProcessesByName("GeometryDash").FirstOrDefault();
            bool running = process != null;
            status.Text = running ? "Geometry Dash: RUNNING" : "Geometry Dash: NOT DETECTED";
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
