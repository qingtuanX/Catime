// PomodoroStats.cs — Catime 番茄钟分项目统计查看器
// 数据来自 %LOCALAPPDATA%\Catime\pomodoro_stats.csv（Catime 每次完成工作时段追加一行）
// 项目列表在 %LOCALAPPDATA%\Catime\pomodoro_projects.txt（一行一个名字，UTF-8）
//
// 用法：
//   pomodoro-stats.exe                 打开统计窗口
//   pomodoro-stats.exe --report        输出文本统计到控制台
//   pomodoro-stats.exe --report-png <文件>   把扇形图渲染成 PNG（无界面）

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Globalization;
using System.IO;
using System.Text;
using System.Windows.Forms;

static class Program
{
    [STAThread]
    static void Main(string[] args)
    {
        string mode = args.Length > 0 ? args[0].ToLowerInvariant() : "";
        if (mode == "--report")
        {
            string report = Stats.BuildTextReport(Stats.LoadAll(), Stats.LoadProjects());
            try { Console.OutputEncoding = Encoding.UTF8; } catch { }
            try { Console.Write(report); } catch { }
            try { System.IO.File.WriteAllText(Path.Combine(Path.GetTempPath(), "pomodoro-stats-report.txt"), report, new UTF8Encoding(false)); } catch { }
            return;
        }
        if (mode == "--report-png" && args.Length > 1)
        {
            List<Record> records = Stats.LoadAll();
            List<string> projects = Stats.LoadProjects();
            Bitmap bitmap = Stats.RenderPieBitmap(records, projects, 900, 620);
            bitmap.Save(args[1], System.Drawing.Imaging.ImageFormat.Png);
            try { Console.Write("saved: " + args[1] + Environment.NewLine); } catch { }
            return;
        }

        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new StatsForm());
    }
}

class Record
{
    public DateTime When;
    public string Project;
    public int Seconds;
}

static class Stats
{
    static string DataDir()
    {
        // 允许用环境变量覆盖（测试/迁移用），默认跟随 Catime 的数据目录
        string overrideDir = Environment.GetEnvironmentVariable("POMODORO_STATS_DIR");
        if (overrideDir != null && overrideDir.Length > 0) return overrideDir;
        string dir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Catime");
        return dir;
    }

    public static string StatsFile() { return Path.Combine(DataDir(), "pomodoro_stats.csv"); }
    public static string ProjectsFile() { return Path.Combine(DataDir(), "pomodoro_projects.txt"); }

    public static List<string> LoadProjects()
    {
        List<string> result = new List<string>();
        try
        {
            if (!File.Exists(ProjectsFile())) return result;
            foreach (string raw in File.ReadAllLines(ProjectsFile(), Encoding.UTF8))
            {
                string line = raw.Trim();
                if (line.Length == 0 || line.StartsWith("#") || line.StartsWith(";")) continue;
                if (!result.Contains(line)) result.Add(line);
            }
        }
        catch { }
        return result;
    }

    public static void SaveProjects(List<string> projects)
    {
        try
        {
            Directory.CreateDirectory(DataDir());
            StringBuilder sb = new StringBuilder();
            sb.Append("# One project name per line (UTF-8).\n");
            foreach (string name in projects) sb.Append(name).Append('\n');
            File.WriteAllText(ProjectsFile(), sb.ToString(), new UTF8Encoding(false));
        }
        catch { }
    }

    public static List<Record> LoadAll()
    {
        List<Record> result = new List<Record>();
        try
        {
            if (!File.Exists(StatsFile())) return result;
            foreach (string raw in File.ReadAllLines(StatsFile(), Encoding.UTF8))
            {
                string line = raw.Trim();
                if (line.Length == 0 || line[0] == '#') continue;
                string[] parts = line.Split(',');
                if (parts.Length < 3) continue;
                Record record = new Record();
                DateTime when;
                if (!DateTime.TryParseExact(parts[0].Trim(), "yyyy-MM-dd HH:mm:ss",
                        CultureInfo.InvariantCulture, DateTimeStyles.None, out when)) continue;
                int seconds;
                if (!int.TryParse(parts[parts.Length - 1].Trim(), out seconds) || seconds <= 0) continue;
                string project = string.Join(",", parts, 1, parts.Length - 2).Trim();
                if (project.Length == 0) project = "Default";
                record.When = when;
                record.Project = project;
                record.Seconds = seconds;
                result.Add(record);
            }
        }
        catch { }
        return result;
    }

    public class Row
    {
        public string Project;
        public int Count;
        public int Seconds;
        public double Share;
        public Color Color;
    }

    public static List<Row> Aggregate(List<Record> records, DateTime? from)
    {
        Dictionary<string, Row> map = new Dictionary<string, Row>();
        foreach (Record record in records)
        {
            if (from.HasValue && record.When < from.Value) continue;
            Row row;
            if (!map.TryGetValue(record.Project, out row))
            {
                row = new Row();
                row.Project = record.Project;
                map[record.Project] = row;
            }
            row.Count++;
            row.Seconds += record.Seconds;
        }

        List<Row> rows = new List<Row>(map.Values);
        int total = 0;
        foreach (Row row in rows) total += row.Seconds;
        foreach (Row row in rows)
        {
            row.Share = total > 0 ? (double)row.Seconds / total : 0;
        }
        rows.Sort(delegate(Row a, Row b) { return b.Seconds.CompareTo(a.Seconds); });

        Color[] palette = new Color[]
        {
            Color.FromArgb(255, 107, 91), Color.FromArgb(78, 203, 113), Color.FromArgb(86, 156, 214),
            Color.FromArgb(220, 170, 60), Color.FromArgb(180, 120, 220), Color.FromArgb(90, 200, 210),
            Color.FromArgb(230, 120, 170), Color.FromArgb(150, 160, 180)
        };
        for (int i = 0; i < rows.Count; i++) rows[i].Color = palette[i % palette.Length];
        return rows;
    }

    public static string FormatDuration(int seconds)
    {
        int minutes = seconds / 60;
        if (minutes < 60) return minutes + " 分钟";
        int hours = minutes / 60;
        int rest = minutes % 60;
        return rest == 0 ? hours + " 小时" : hours + " 小时 " + rest + " 分";
    }

    public static string BuildTextReport(List<Record> records, List<string> projects)
    {
        StringBuilder sb = new StringBuilder();
        sb.AppendLine("Catime 番茄钟统计");
        sb.AppendLine("数据文件: " + StatsFile());
        sb.AppendLine();

        List<Row> all = Aggregate(records, null);
        DateTime today = DateTime.Today;
        List<Row> todayRows = Aggregate(records, today);

        sb.AppendLine("== 全部 ==");
        AppendRows(sb, all);
        sb.AppendLine();
        sb.AppendLine("== 今天 ==");
        AppendRows(sb, todayRows);
        sb.AppendLine();
        sb.AppendLine("项目列表: " + string.Join(", ", projects.ToArray()));
        return sb.ToString();
    }

    static void AppendRows(StringBuilder sb, List<Row> rows)
    {
        if (rows.Count == 0)
        {
            sb.AppendLine("  (暂无记录)");
            return;
        }
        int totalCount = 0;
        int totalSeconds = 0;
        foreach (Row row in rows)
        {
            totalCount += row.Count;
            totalSeconds += row.Seconds;
            sb.AppendLine(string.Format(CultureInfo.InvariantCulture,
                "  {0,-16} {1,3} 个   {2,12}   {3,6:0.0}%",
                row.Project, row.Count, FormatDuration(row.Seconds), row.Share * 100));
        }
        sb.AppendLine(string.Format(CultureInfo.InvariantCulture,
            "  合计: {0} 个, {1}", totalCount, FormatDuration(totalSeconds)));
    }

    public static void DrawPie(Graphics g, Rectangle area, List<Row> rows, string title)
    {
        g.SmoothingMode = SmoothingMode.AntiAlias;
        using (Font titleFont = new Font("Microsoft YaHei UI", 11f, FontStyle.Bold))
        using (Font textFont = new Font("Microsoft YaHei UI", 9.5f))
        using (Brush textBrush = new SolidBrush(Color.FromArgb(230, 232, 236)))
        using (Brush dimBrush = new SolidBrush(Color.FromArgb(150, 156, 164)))
        {
            g.DrawString(title, titleFont, textBrush, area.X + 4, area.Y);

            int size = Math.Min(area.Width - 260, area.Height - 60);
            if (size < 80) size = 80;
            Rectangle pieRect = new Rectangle(area.X + 10, area.Y + 40, size, size);

            if (rows.Count == 0)
            {
                g.DrawString("暂无数据 — 完成一个番茄后这里会有统计", textFont, dimBrush,
                    area.X + 10, area.Y + 60);
                return;
            }

            float startAngle = -90f;
            foreach (Row row in rows)
            {
                float sweep = (float)(row.Share * 360.0);
                if (sweep < 0.35f) sweep = 0.35f;
                using (Brush slice = new SolidBrush(row.Color))
                {
                    g.FillPie(slice, pieRect, startAngle, sweep);
                }
                using (Pen edge = new Pen(Color.FromArgb(36, 38, 42), 1.5f))
                {
                    g.DrawPie(edge, pieRect, startAngle, sweep);
                }
                startAngle += sweep;
            }

            int legendX = pieRect.Right + 24;
            int legendY = area.Y + 46;
            foreach (Row row in rows)
            {
                using (Brush swatch = new SolidBrush(row.Color))
                {
                    g.FillRectangle(swatch, legendX, legendY + 3, 12, 12);
                }
                string label = string.Format(CultureInfo.InvariantCulture,
                    "{0}  ·  {1}  ·  {2:0.0}%", row.Project, FormatDuration(row.Seconds), row.Share * 100);
                g.DrawString(label, textFont, textBrush, legendX + 20, legendY);
                legendY += 24;
                if (legendY > area.Bottom - 24) break;
            }
        }
    }

    public static Bitmap RenderPieBitmap(List<Record> records, List<string> projects, int width, int height)
    {
        Bitmap bitmap = new Bitmap(width, height);
        using (Graphics g = Graphics.FromImage(bitmap))
        {
            g.Clear(Color.FromArgb(32, 34, 37));
            List<Row> rows = Aggregate(records, null);
            DrawPie(g, new Rectangle(10, 10, width - 20, height - 20), rows, "按时间占比（全部）");
        }
        return bitmap;
    }
}

class StatsForm : Form
{
    ListView table;
    Panel chart;
    Label summary;
    ComboBox rangeBox;
    Timer refreshTimer;
    List<Stats.Row> currentRows = new List<Stats.Row>();
    List<Record> allRecords = new List<Record>();
    List<string> projects = new List<string>();

    public StatsForm()
    {
        Text = "Catime 番茄钟统计";
        StartPosition = FormStartPosition.CenterScreen;
        ClientSize = new Size(940, 560);
        MinimumSize = new Size(760, 460);
        BackColor = Color.FromArgb(32, 34, 37);
        ForeColor = Color.FromArgb(230, 232, 236);
        Font = new Font("Microsoft YaHei UI", 9.5f);

        chart = new Panel();
        chart.Dock = DockStyle.Fill;
        chart.BackColor = Color.FromArgb(32, 34, 37);
        chart.Paint += delegate(object sender, PaintEventArgs e)
        {
            Stats.DrawPie(e.Graphics, chart.ClientRectangle, currentRows, "按时间占比");
        };
        Controls.Add(chart);

        Panel toolbar = new Panel();
        toolbar.Dock = DockStyle.Top;
        toolbar.Height = 44;
        toolbar.BackColor = Color.FromArgb(40, 42, 46);
        Controls.Add(toolbar);

        Label rangeLabel = new Label();
        rangeLabel.Text = "时间段";
        rangeLabel.AutoSize = true;
        rangeLabel.Location = new Point(12, 13);
        toolbar.Controls.Add(rangeLabel);

        rangeBox = new ComboBox();
        rangeBox.DropDownStyle = ComboBoxStyle.DropDownList;
        rangeBox.Items.AddRange(new object[] { "全部", "今天", "近 7 天" });
        rangeBox.SelectedIndex = 0;
        rangeBox.Location = new Point(66, 9);
        rangeBox.Width = 110;
        rangeBox.SelectedIndexChanged += delegate { RefreshData(); };
        toolbar.Controls.Add(rangeBox);

        Button refresh = new Button();
        refresh.Text = "刷新";
        refresh.Location = new Point(190, 8);
        refresh.Width = 70;
        refresh.Click += delegate { RefreshData(); };
        toolbar.Controls.Add(refresh);

        Button manage = new Button();
        manage.Text = "项目管理";
        manage.Location = new Point(268, 8);
        manage.Width = 90;
        manage.Click += delegate { ManageProjects(); };
        toolbar.Controls.Add(manage);

        Button openFolder = new Button();
        openFolder.Text = "打开数据文件夹";
        openFolder.Location = new Point(366, 8);
        openFolder.Width = 120;
        openFolder.Click += delegate
        {
            try { System.Diagnostics.Process.Start("explorer.exe", Path.GetDirectoryName(Stats.StatsFile())); } catch { }
        };
        toolbar.Controls.Add(openFolder);

        summary = new Label();
        summary.AutoSize = false;
        summary.AutoEllipsis = true;
        summary.Dock = DockStyle.Bottom;
        summary.Height = 30;
        summary.TextAlign = ContentAlignment.MiddleLeft;
        summary.Padding = new Padding(12, 0, 0, 0);
        summary.BackColor = Color.FromArgb(40, 42, 46);
        Controls.Add(summary);

        table = new ListView();
        table.View = View.Details;
        table.FullRowSelect = true;
        table.Dock = DockStyle.Left;
        table.Width = 420;
        table.BackColor = Color.FromArgb(36, 38, 42);
        table.ForeColor = Color.FromArgb(230, 232, 236);
        table.Columns.Add("项目", 150);
        table.Columns.Add("番茄数", 70, HorizontalAlignment.Right);
        table.Columns.Add("总时长", 120, HorizontalAlignment.Right);
        table.Columns.Add("占比", 70, HorizontalAlignment.Right);
        Controls.Add(table);


        refreshTimer = new Timer();
        refreshTimer.Interval = 2000;
        refreshTimer.Tick += delegate { ReloadData(); };
        refreshTimer.Start();

        Load += delegate { ReloadData(); };
    }

    void ReloadData()
    {
        allRecords = Stats.LoadAll();
        projects = Stats.LoadProjects();
        RefreshData();
    }

    void RefreshData()
    {
        DateTime? from = null;
        if (rangeBox.SelectedIndex == 1) from = DateTime.Today;
        else if (rangeBox.SelectedIndex == 2) from = DateTime.Today.AddDays(-6);

        currentRows = Stats.Aggregate(allRecords, from);

        table.BeginUpdate();
        table.Items.Clear();
        int totalCount = 0;
        int totalSeconds = 0;
        foreach (Stats.Row row in currentRows)
        {
            totalCount += row.Count;
            totalSeconds += row.Seconds;
            ListViewItem item = new ListViewItem(row.Project);
            item.SubItems.Add(row.Count.ToString(CultureInfo.InvariantCulture));
            item.SubItems.Add(Stats.FormatDuration(row.Seconds));
            item.SubItems.Add((row.Share * 100).ToString("0.0", CultureInfo.InvariantCulture) + "%");
            item.ForeColor = row.Color;
            table.Items.Add(item);
        }
        table.EndUpdate();

        summary.Text = string.Format(CultureInfo.InvariantCulture,
            "共 {0} 个番茄 · 总计 {1} · 项目 {2} 个", totalCount,
            Stats.FormatDuration(totalSeconds), currentRows.Count);
        summary.Text += "    （数据文件：" + Stats.StatsFile() + "）";

        chart.Invalidate();
    }

    void ManageProjects()
    {
        ProjectsForm dialog = new ProjectsForm(projects);
        if (dialog.ShowDialog(this) == DialogResult.OK)
        {
            Stats.SaveProjects(dialog.Projects);
            ReloadData();
        }
    }
}

class ProjectsForm : Form
{
    public List<string> Projects = new List<string>();
    ListBox list;
    TextBox input;

    public ProjectsForm(List<string> current)
    {
        foreach (string name in current) Projects.Add(name);

        Text = "项目管理";
        StartPosition = FormStartPosition.CenterParent;
        ClientSize = new Size(360, 340);
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = false;
        MinimizeBox = false;
        Font = new Font("Microsoft YaHei UI", 10f);

        list = new ListBox();
        list.Location = new Point(16, 16);
        list.Size = new Size(328, 210);
        Controls.Add(list);

        input = new TextBox();
        input.Location = new Point(16, 240);
        input.Width = 200;
        Controls.Add(input);

        Button add = new Button();
        add.Text = "新增";
        add.Location = new Point(226, 238);
        add.Width = 56;
        add.Click += delegate
        {
            string name = input.Text.Trim();
            if (name.Length == 0 || Projects.Contains(name)) return;
            Projects.Add(name);
            input.Text = "";
            RefreshList();
        };
        Controls.Add(add);

        Button remove = new Button();
        remove.Text = "删除";
        remove.Location = new Point(288, 238);
        remove.Width = 56;
        remove.Click += delegate
        {
            if (list.SelectedIndex < 0) return;
            Projects.RemoveAt(list.SelectedIndex);
            RefreshList();
        };
        Controls.Add(remove);

        Label hint = new Label();
        hint.Text = "项目名会出现在 Catime 托盘菜单的 Pomodoro → Project 里";
        hint.AutoSize = true;
        hint.Location = new Point(16, 272);
        hint.ForeColor = Color.Gray;
        Controls.Add(hint);

        Button ok = new Button();
        ok.Text = "保存";
        ok.DialogResult = DialogResult.OK;
        ok.Location = new Point(190, 298);
        ok.Width = 74;
        Controls.Add(ok);

        Button cancel = new Button();
        cancel.Text = "取消";
        cancel.DialogResult = DialogResult.Cancel;
        cancel.Location = new Point(270, 298);
        cancel.Width = 74;
        Controls.Add(cancel);

        AcceptButton = ok;
        CancelButton = cancel;
        RefreshList();
    }

    void RefreshList()
    {
        list.Items.Clear();
        foreach (string name in Projects) list.Items.Add(name);
    }
}
