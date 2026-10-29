using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Globalization;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Windows.Forms;

[assembly: AssemblyTitle("openOfficeTab Setup")]
[assembly: AssemblyDescription("Install openOfficeTab for the current Windows user")]
[assembly: AssemblyProduct("openOfficeTab")]
[assembly: AssemblyVersion("0.1.9.0")]
[assembly: AssemblyFileVersion("0.1.9.0")]

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
                    if (archive.GetEntry("scripts/install.ps1") == null || archive.GetEntry("scripts/uninstall.ps1") == null || archive.GetEntry("scripts/desktop-context.ps1") == null || archive.GetEntry("scripts/diagnose.ps1") == null || archive.GetEntry("scripts/localization.ps1") == null)
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
        private readonly Button diagnose = new Button();
        private readonly Button repair = new Button();
        private readonly TextBox details = new TextBox();
        private readonly Label subtitle = new Label();
        private readonly Label instructions = new Label();
        private readonly Label languageCaption = new Label();
        private readonly ComboBox languageChoice = new ComboBox();
        private bool busy;
        private bool English { get { return languageChoice.SelectedIndex == 2 || (languageChoice.SelectedIndex <= 0 && CultureInfo.CurrentUICulture.TwoLetterISOLanguageName != "zh"); } }
        private string T(string chinese, string english) { return English ? english : chinese; }
        private void ApplyLanguage() {
            Text = T("openOfficeTab · 安装与恢复", "openOfficeTab · Setup and recovery");
            subtitle.Text = T("Word / Excel / PowerPoint 文档标签 · 0.1.9 · x64", "Document tabs for Word / Excel / PowerPoint · 0.1.9 · x64");
            instructions.Text = T("安装、卸载或修复前，请保存并退出三款 Office；加载诊断可直接运行。\n按当前用户安装，无需管理员权限；完成后重新打开文档。", "Save and exit all three Office apps before installing, removing, or repairing.\nDiagnostics can run at any time. Installation is per user; no administrator needed.");
            languageCaption.Text = T("安装器语言", "Installer language");
            install.Text = T("安装 / 更新", "Install / Update");
            uninstall.Text = T("卸载插件", "Uninstall");
            diagnose.Text = T("加载诊断", "Diagnostics");
            repair.Text = T("修复注册", "Repair registration");
            details.Text = T("设置默认保留。插件使用 MIT 许可证，安装文件不修改 Office 安全策略。", "Settings are kept by default. MIT licensed. The installer does not change Office security policy.");
        }
        internal SetupWindow() {
            Text = "openOfficeTab · 安装与恢复";
            ClientSize = new Size(640, 454);
            Font = new Font("Segoe UI", 10);
            AutoScaleMode = AutoScaleMode.Dpi;
            StartPosition = FormStartPosition.CenterScreen;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            Controls.Add(new Label { Text = "openOfficeTab", Font = new Font(Font.FontFamily, 23, FontStyle.Bold), Location = new Point(24, 18), AutoSize = true });
            subtitle.SetBounds(26, 68, 588, 26);
            instructions.SetBounds(26, 103, 588, 58);
            languageCaption.SetBounds(26, 169, 145, 26);
            languageChoice.SetBounds(178, 166, 270, 28);
            languageChoice.DropDownStyle = ComboBoxStyle.DropDownList;
            languageChoice.Items.AddRange(new object[] { "跟随系统 / System default", "简体中文", "English" });
            languageChoice.SelectedIndex = 0;
            languageChoice.SelectedIndexChanged += delegate { ApplyLanguage(); };
            Controls.Add(subtitle); Controls.Add(instructions); Controls.Add(languageCaption); Controls.Add(languageChoice);
            install.Text = "安装 / 更新";
            install.SetBounds(26, 214, 136, 40);
            install.Click += delegate { Run(0); };
            uninstall.Text = "卸载插件";
            uninstall.SetBounds(178, 214, 116, 40);
            uninstall.Click += delegate { Run(1); };
            diagnose.Text = "加载诊断";
            diagnose.SetBounds(310, 214, 124, 40);
            diagnose.Click += delegate { Run(2); };
            repair.Text = "修复注册";
            repair.SetBounds(450, 214, 164, 40);
            repair.Click += delegate { Run(3); };
            Controls.Add(install);
            Controls.Add(uninstall);
            Controls.Add(diagnose);
            Controls.Add(repair);
            details.SetBounds(26, 272, 588, 158);
            details.Multiline = true;
            details.ReadOnly = true;
            details.ScrollBars = ScrollBars.Vertical;
            details.Text = "设置默认保留。插件使用 MIT 许可证，安装文件不修改 Office 安全策略。";
            Controls.Add(details);
            ApplyLanguage();
        }
        protected override void OnFormClosing(FormClosingEventArgs e) {
            if (busy) e.Cancel = true;
            base.OnFormClosing(e);
        }
        private void Run(int operation) {
            busy = true;
            install.Enabled = uninstall.Enabled = diagnose.Enabled = repair.Enabled = false;
            languageChoice.Enabled = false;
            details.Text = T("正在处理，请稍候……", "Working, please wait…");
            string language = English ? "en" : "zh-CN";
            BackgroundWorker worker = new BackgroundWorker();
            worker.DoWork += delegate(object sender, DoWorkEventArgs e) { e.Result = Execute(operation, language); };
            worker.RunWorkerCompleted += delegate(object sender, RunWorkerCompletedEventArgs e) {
                details.Text = e.Error != null ? e.Error.Message : (string)e.Result;
                busy = false;
                install.Enabled = uninstall.Enabled = diagnose.Enabled = repair.Enabled = true;
                languageChoice.Enabled = true;
                worker.Dispose();
            };
            worker.RunWorkerAsync();
        }
        private static string Execute(int operation, string language) {
            Func<string, string, string> t = delegate(string zh, string en) { return language == "en" ? en : zh; };
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
                string script = Path.Combine(directory, "scripts", operation >= 2 ? "diagnose.ps1" : operation == 1 ? "uninstall.ps1" : "install.ps1");
                ProcessStartInfo info = new ProcessStartInfo(executable, "-NoProfile -ExecutionPolicy Bypass -File \"" + script + "\" -Language " + language + (operation == 3 ? " -Repair" : ""));
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
                    if (process.ExitCode != 0) return t("未完成，请检查以下信息：\r\n", "Not completed. Check the details below:\r\n") + error + output;
                    return (operation == 1 ? t("卸载完成。\r\n", "Uninstall complete.\r\n") : operation == 2 ? t("诊断完成。\r\n", "Diagnostics complete.\r\n") : t("安装 / 注册修复完成。重新打开 Office 文档即可使用标签。\r\n", "Installation / registration repair complete. Reopen Office to use document tabs.\r\n")) + output;
                }
            } finally {
                // Remove only the randomly named extraction directory created above.
                try { Directory.Delete(directory, true); } catch { }
            }
        }
    }
}
