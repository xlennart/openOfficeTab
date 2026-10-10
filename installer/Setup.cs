using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;

[assembly: AssemblyTitle("openOfficetab Setup")]
[assembly: AssemblyDescription("Install openOfficetab for the current Windows user")]
[assembly: AssemblyProduct("openOfficetab")]
[assembly: AssemblyVersion("0.1.1.0")]
[assembly: AssemblyFileVersion("0.1.1.0")]

internal static class Setup {
    private const string PayloadName = "openOfficetab.Payload.zip";
    [STAThread]
    private static void Main(string[] args) {
        if (args.Length == 2 && args[0] == "--verify") {
            try {
                using (Stream resource = Assembly.GetExecutingAssembly().GetManifestResourceStream(PayloadName))
                using (ZipArchive archive = new ZipArchive(resource, ZipArchiveMode.Read))
                using (Stream dll = archive.GetEntry("dist/x64/openOfficetab.dll").Open())
                using (SHA256 sha = SHA256.Create()) {
                    string hash = BitConverter.ToString(sha.ComputeHash(dll)).Replace("-", "");
                    if (archive.GetEntry("scripts/install.ps1") == null || archive.GetEntry("scripts/uninstall.ps1") == null || archive.GetEntry("scripts/desktop-context.ps1") == null)
                        throw new InvalidDataException("Installer payload is incomplete.");
                    File.WriteAllText(args[1], hash + Environment.NewLine, Encoding.ASCII);
                }
            } catch { Environment.ExitCode = 1; }
            return;
        }
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new SetupWindow());
    }

    private sealed class SetupWindow : Form {
        private readonly Button install = new Button();
        private readonly Button uninstall = new Button();
        private readonly TextBox details = new TextBox();
        private bool busy;
        internal SetupWindow() {
            Text = "openOfficetab · 安装与卸载";
            ClientSize = new Size(600, 410);
            Font = new Font("Microsoft YaHei UI", 10);
            StartPosition = FormStartPosition.CenterScreen;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            Controls.Add(new Label { Text = "openOfficetab", Font = new Font(Font.FontFamily, 23, FontStyle.Bold), Location = new Point(24, 18), AutoSize = true });
            Controls.Add(new Label { Text = "Word / Excel / PowerPoint 文档标签 · 0.1.1 · x64", Location = new Point(26, 68), AutoSize = true });
            Controls.Add(new Label { Text = "请先保存文档并退出三款 Office。\n按当前用户安装，无需管理员权限；安装完成后重新打开文档。", Location = new Point(26, 103), Size = new Size(550, 55) });
            install.Text = "安装 / 更新";
            install.SetBounds(26, 170, 170, 40);
            install.Click += delegate { Run(false); };
            uninstall.Text = "卸载插件";
            uninstall.SetBounds(210, 170, 140, 40);
            uninstall.Click += delegate { Run(true); };
            Controls.Add(install);
            Controls.Add(uninstall);
            details.SetBounds(26, 228, 548, 158);
            details.Multiline = true;
            details.ReadOnly = true;
            details.ScrollBars = ScrollBars.Vertical;
            details.Text = "设置默认保留。插件使用 MIT 许可证，安装文件不修改 Office 安全策略。";
            Controls.Add(details);
        }
        protected override void OnFormClosing(FormClosingEventArgs e) {
            if (busy) e.Cancel = true;
            base.OnFormClosing(e);
        }
        private void Run(bool remove) {
            busy = true;
            install.Enabled = uninstall.Enabled = false;
            details.Text = "正在处理，请稍候……";
            BackgroundWorker worker = new BackgroundWorker();
            worker.DoWork += delegate(object sender, DoWorkEventArgs e) { e.Result = Execute(remove); };
            worker.RunWorkerCompleted += delegate(object sender, RunWorkerCompletedEventArgs e) {
                details.Text = e.Error != null ? e.Error.Message : (string)e.Result;
                busy = false;
                install.Enabled = uninstall.Enabled = true;
                worker.Dispose();
            };
            worker.RunWorkerAsync();
        }
        private static string Execute(bool remove) {
            string directory = Path.Combine(Path.GetTempPath(), "openOfficetab-setup-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            try {
                using (Stream resource = Assembly.GetExecutingAssembly().GetManifestResourceStream(PayloadName))
                using (ZipArchive archive = new ZipArchive(resource, ZipArchiveMode.Read)) {
                    string prefix = Path.GetFullPath(directory) + Path.DirectorySeparatorChar;
                    foreach (ZipArchiveEntry entry in archive.Entries) {
                        string destination = Path.GetFullPath(Path.Combine(directory, entry.FullName));
                        if (!destination.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Unexpected payload path.");
                        if (entry.Name.Length == 0) { Directory.CreateDirectory(destination); continue; }
                        Directory.CreateDirectory(Path.GetDirectoryName(destination));
                        using (Stream input = entry.Open())
                        using (FileStream output = File.Create(destination)) input.CopyTo(output);
                    }
                }
                string executable = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "System32", "WindowsPowerShell", "v1.0", "powershell.exe");
                if (!Environment.Is64BitProcess && Environment.Is64BitOperatingSystem)
                    executable = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "Sysnative", "WindowsPowerShell", "v1.0", "powershell.exe");
                string script = Path.Combine(directory, "scripts", remove ? "uninstall.ps1" : "install.ps1");
                ProcessStartInfo info = new ProcessStartInfo(executable, "-NoProfile -ExecutionPolicy Bypass -File \"" + script + "\"");
                info.UseShellExecute = false;
                info.CreateNoWindow = true;
                info.RedirectStandardOutput = info.RedirectStandardError = true;
                info.StandardOutputEncoding = info.StandardErrorEncoding = Encoding.UTF8;
                info.EnvironmentVariables["OPENOFFICETAB_SETUP_UTF8"] = "1";
                using (Process process = new Process()) {
                    StringBuilder error = new StringBuilder();
                    process.StartInfo = info;
                    process.ErrorDataReceived += delegate(object sender, DataReceivedEventArgs e) { if (e.Data != null) error.AppendLine(e.Data); };
                    process.Start();
                    process.BeginErrorReadLine();
                    string output = process.StandardOutput.ReadToEnd();
                    process.WaitForExit();
                    if (process.ExitCode != 0) return "未完成，请检查以下信息：\r\n" + error + output;
                    return (remove ? "卸载完成。\r\n" : "安装完成。重新打开 Office 文档即可使用标签。\r\n") + output;
                }
            } finally {
                // Remove only the randomly named extraction directory created above.
                try { Directory.Delete(directory, true); } catch { }
            }
        }
    }
}
