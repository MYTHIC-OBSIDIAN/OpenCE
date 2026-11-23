// OpenCE Achievements - Xbox-style achievement pop-ups and list for OpenCE (Halo: Combat Evolved).
//
//  * Runs quietly in the background while you play (the game starts it with --background when it is
//    next to halo.exe: the game.achievements_app setting).
//  * The game appends a 16-byte record to achievements.log in its save root's u folder every time
//    it earns an achievement: u32 achievement number, u32 0, u64 Windows FILETIME
//    (port/linux/game/achievements.c). This program watches that file and shows the
//    "Achievement unlocked" pop-up with a sound.
//  * Opened normally it shows your achievement list (37 achievements, 780G, Data.cs).
//  * The list: up and down (keys, D-pad, left stick) move the selection, Page Up / Page Down and the
//    triggers a page, left and right (D-pad) or Y the order, Tab and the bumpers the filter, Escape / B /
//    Back closes it.
//    scroll.wav plays as the list scrolls or the selection moves, filter.wav as the order, filter or picture
//    changes, window.wav once as the window opens or closes.
//  * Pictures: any picture named after the achievement in an "Achievement Icons" folder next to this
//    program or the game is used, else the one built in from assets\icons\<number>.png (Build.bat).
//
// C# 5 / .NET Framework 4 (builds with the csc.exe that ships with Windows - see Build.bat).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;

namespace OpenCEAchievements
{
    class Achievement
    {
        public readonly int Id;               // Xbox 360 achievement id
        public readonly string Name, Desc;
        public readonly int Score;
        public readonly long Unlocked360;     // FILETIME, 0 = not unlocked on the 360
        public readonly int[] DeIds;          // Definitive Edition achievement numbers that all unlock this one
        public readonly int[] Requires;       // or: achievements on this list (Xbox 360 ids) that must all be unlocked first
        public long UnlockedAt;               // FILETIME of the unlock shown in the list (0 = locked)
        public bool Unlocked { get { return UnlockedAt != 0; } }

        public Achievement(int id, string name, string desc, int score, long unlocked360, int[] deIds)
            : this(id, name, desc, score, unlocked360, deIds, new int[0]) { }

        public Achievement(int id, string name, string desc, int score, long unlocked360, int[] deIds, int[] requires)
        {
            Id = id; Name = name; Desc = desc; Score = score; Unlocked360 = unlocked360; DeIds = deIds; Requires = requires; UnlockedAt = unlocked360;
        }

        public string FileName
        {
            get
            {
                var sb = new StringBuilder();
                foreach (char c in Name) if (Array.IndexOf(Path.GetInvalidFileNameChars(), c) < 0) sb.Append(c);
                return sb.ToString().Trim();
            }
        }

        public DateTime When { get { return DateTime.FromFileTimeUtc(UnlockedAt).ToLocalTime(); } }
    }

    static class Program
    {
        public const string ShowEventName = "OpenCEAchievements.Show";
        public static bool Background;

        [STAThread]
        static void Main(string[] args)
        {
            foreach (var a in args) if (a.Equals("--background", StringComparison.OrdinalIgnoreCase)) Background = true;

            bool first;
            using (var mutex = new Mutex(true, "OpenCEAchievements.Instance", out first))
            {
                if (!first)
                {
                    // Already running: bring its list up (unless this is just the launcher making sure it runs).
                    if (!Background)
                    {
                        try { using (var ev = EventWaitHandle.OpenExisting(ShowEventName)) ev.Set(); } catch { }
                    }
                    return;
                }
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.ThreadException += delegate(object s, ThreadExceptionEventArgs e) { Fatal(e.Exception); };
                try { Application.Run(new Handler()); }
                catch (Exception e) { Fatal(e); }
            }
        }

        static void Fatal(Exception e)
        {
            MessageBox.Show("OpenCE Achievements hit an unexpected error:\n\n" + e.Message, "OpenCE", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    // ------------------------------------------------------------------ files

    static class Game
    {
        public static readonly string Root = FindRoot();

        // The game's save roots: paths.saves in config.toml, else a savegame folder next to halo.exe, else
        // %APPDATA%\halo (when the game could not write next to halo.exe). All are read.
        public static string[] SaveRoots
        {
            get
            {
                var list = new List<string>();
                string configured = ConfiguredSaves();
                if (!string.IsNullOrEmpty(configured)) list.Add(configured);
                list.Add(Path.Combine(Root, "savegame"));
                list.Add(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "halo"));
                return list.ToArray();
            }
        }

        // achievements.log in each save root's u folder (the game's u:\ drive)
        public static string[] LogPaths
        {
            get
            {
                var roots = SaveRoots;
                var paths = new string[roots.Length];
                for (int i = 0; i < roots.Length; i++) paths[i] = Path.Combine(Path.Combine(roots[i], "u"), "achievements.log");
                return paths;
            }
        }

        // paths.saves = "..." in config.toml next to halo.exe, if set
        static string ConfiguredSaves()
        {
            try
            {
                string config = Path.Combine(Root, "config.toml");
                if (!File.Exists(config)) return null;
                string section = "";
                foreach (var raw in File.ReadAllLines(config))
                {
                    string line = raw.Trim();
                    if (line.StartsWith("[")) { section = line.Trim('[', ']').Trim(); continue; }
                    string key = null;
                    if (section == "paths" && line.StartsWith("saves")) key = line.Substring(5);
                    else if (section == "" && line.StartsWith("paths.saves")) key = line.Substring(11);
                    if (key == null) continue;
                    key = key.Trim();
                    if (!key.StartsWith("=")) continue;
                    string value = key.Substring(1).Trim();
                    int hash = value.IndexOf('#');
                    if (value.StartsWith("\"")) { int close = value.IndexOf('"', 1); value = close > 0 ? value.Substring(1, close - 1) : ""; }
                    else if (hash >= 0) value = value.Substring(0, hash).Trim();
                    value = value.Replace("\\\\", "\\");
                    if (value.Length == 0) return null;
                    return Path.IsPathRooted(value) ? value : Path.Combine(Root, value);
                }
            }
            catch { }
            return null;
        }

        static string FindRoot()
        {
            string d = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');
            for (int i = 0; i < 3 && !string.IsNullOrEmpty(d); i++)
            {
                if (File.Exists(Path.Combine(d, "halo.exe"))) return d;
                d = Path.GetDirectoryName(d);
            }
            // not next to this exe: the folder of the running game
            try
            {
                foreach (var p in Process.GetProcessesByName("halo"))
                    using (p) return Path.GetDirectoryName(p.MainModule.FileName);
            }
            catch { }
            return AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');
        }

        // achievements_profile.txt in a save root's u folder (the newest of them), which the game writes as
        // its players' profiles change: player 1's profile's name, then each local player's log (from the u
        // folder; player 1's first). Null until the game has written one.
        static string[] ProfileLines(out string uFolder)
        {
            string[] lines = null;
            uFolder = null;
            DateTime newest = DateTime.MinValue;
            foreach (var root in SaveRoots)
            {
                try
                {
                    string u = Path.Combine(root, "u");
                    string p = Path.Combine(u, "achievements_profile.txt");
                    if (!File.Exists(p)) continue;
                    DateTime when = File.GetLastWriteTimeUtc(p);
                    if (when <= newest) continue;
                    var read = File.ReadAllText(p, System.Text.Encoding.UTF8).Replace("\r", "").Split('\n');
                    if (read.Length == 0 || read[0].Trim().Length == 0) continue;
                    lines = read; uFolder = u; newest = when;
                }
                catch { }
            }
            return lines;
        }

        // The title of the list's header: the name of the profile player 1 plays on, else "Achievements".
        public static string Gamertag()
        {
            string u;
            var lines = ProfileLines(out u);
            return lines != null ? lines[0].Trim() : "Achievements";
        }

        // The local players' logs, player 1's first (the list's; anyone's earns a pop-up). Null before the game
        // has named them: then the one log of every profile, achievements.log in the u folders.
        public static string[] ProfileLogs()
        {
            string u;
            var lines = ProfileLines(out u);
            if (lines == null) return null;
            var logs = new List<string>();
            for (int i = 1; i < lines.Length; i++)
            {
                string rel = lines[i].Trim();
                if (rel.Length == 0) continue;
                logs.Add(Path.Combine(u, rel.Replace('/', '\\')));
            }
            return logs.Count > 0 ? logs.ToArray() : null;
        }

        // The header's picture chosen, per profile: achievements_gamerpic.txt beside the profile's log (in its
        // folder; achievements_default_0_gamerpic.txt beside a built-in profile's), so it goes where the
        // profile's achievements go (deleting the profile deletes it). Before the game names a profile, beside
        // this program.
        static string GamerpicChoicePath()
        {
            var logs = ProfileLogs();
            if (logs == null || logs.Length == 0)
                return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "achievements_gamerpic.txt");
            string log = logs[0];
            return Path.Combine(Path.GetDirectoryName(log), Path.GetFileNameWithoutExtension(log) + "_gamerpic.txt");
        }

        public static int GamerpicChoice()
        {
            try
            {
                string p = GamerpicChoicePath();
                int n;
                if (File.Exists(p) && int.TryParse(File.ReadAllText(p).Trim(), out n) && n >= 0) return n;
            }
            catch { }
            return 0;
        }

        public static void SaveGamerpicChoice(int n)
        {
            try
            {
                string p = GamerpicChoicePath();
                Directory.CreateDirectory(Path.GetDirectoryName(p));
                File.WriteAllText(p, n.ToString());
            }
            catch { }
        }

        // Gamerpic.png (or .jpg, .bmp) next to this program, if you put one there.
        public static Image Gamerpic()
        {
            foreach (var ext in new[] { ".png", ".jpg", ".jpeg", ".bmp" })
            {
                string p = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Gamerpic" + ext);
                if (!File.Exists(p)) continue;
                try { using (var s = new MemoryStream(File.ReadAllBytes(p))) using (var img = Image.FromStream(s)) return new Bitmap(img); }
                catch { }
            }
            return null;
        }

        public static bool Running()
        {
            var ps = Process.GetProcessesByName("halo");
            foreach (var p in ps) p.Dispose();
            return ps.Length > 0;
        }

        // Sound for the pop-up: achievement.wav / .mp3 / .wma next to this program or in the game folder,
        // else the one built in (assets\achievement.mp3, Build.bat).
        public static string SoundFile()
        {
            string here = AppDomain.CurrentDomain.BaseDirectory;
            foreach (var dir in new[] { here, Root })
                foreach (var name in new[] { "achievement.wav", "achievement.mp3", "achievement.wma" })
                {
                    string p = Path.Combine(dir, name);
                    if (File.Exists(p)) return p;
                }
            try
            {
                using (var src = Res.Open("achievement.mp3"))
                {
                    if (src == null) return null;
                    string t = Path.Combine(Path.GetTempPath(), "OpenCEAchievements_unlocked.mp3");
                    if (!File.Exists(t) || new FileInfo(t).Length != src.Length)
                        using (var dst = new FileStream(t, FileMode.Create, FileAccess.Write, FileShare.Read)) src.CopyTo(dst);
                    return t;
                }
            }
            catch { return null; }
        }
    }

    // The list's sounds (Games for Windows - LIVE's): scroll.wav (btn_Focus) when the list scrolls or the
    // selection moves, filter.wav (btn_Select) when the order, the filter or the picture changes, window.wav
    // (tab_Switch) once as the window opens or closes. Each is a file of that name next to this program, else the one built in (assets\, Build.bat).
    static class Sfx
    {
        static readonly Dictionary<string, System.Media.SoundPlayer> players = new Dictionary<string, System.Media.SoundPlayer>();

        static System.Media.SoundPlayer Get(string name)
        {
            System.Media.SoundPlayer player;
            if (players.TryGetValue(name, out player)) return player;
            player = null;
            try
            {
                string file = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, name);
                Stream data = null;
                if (File.Exists(file)) data = new MemoryStream(File.ReadAllBytes(file));
                else
                {
                    using (var src = Res.Open(name))
                        if (src != null) { var copy = new MemoryStream(); src.CopyTo(copy); copy.Position = 0; data = copy; }
                }
                if (data != null) { player = new System.Media.SoundPlayer(data); player.Load(); }
            }
            catch { player = null; }
            players[name] = player;
            return player;
        }

        public static void Play(string name)
        {
            var player = Get(name);
            if (player == null) return;
            try { player.Stop(); player.Play(); } catch { }
        }

        static DateTime lastScroll;
        public static void Scroll()
        {
            if ((DateTime.Now - lastScroll).TotalMilliseconds < 60) return;   // no buzzing on fast drags
            lastScroll = DateTime.Now;
            Play("scroll.wav");
        }
        public static void Filter() { Play("filter.wav"); }
        public static void Window() { Play("window.wav"); }
    }

    // Xbox controllers through XInput (xinput1_4.dll, else xinput9_1_0.dll): every connected one's buttons
    // together, with the left stick's directions as the D-pad's and the triggers as two more buttons.
    static class Pad
    {
        public const int Up = 0x1, Down = 0x2, Left = 0x4, Right = 0x8, Start = 0x10, Back = 0x20,
            LB = 0x100, RB = 0x200, A = 0x1000, B = 0x2000, X = 0x4000, Y = 0x8000, LT = 0x10000, RT = 0x20000;

        [StructLayout(LayoutKind.Sequential)]
        struct XInputState
        {
            public uint Packet;
            public ushort Buttons;
            public byte LeftTrigger, RightTrigger;
            public short LeftX, LeftY, RightX, RightY;
        }

        [DllImport("xinput1_4.dll", EntryPoint = "XInputGetState")] static extern uint GetState14(uint index, out XInputState state);
        [DllImport("xinput9_1_0.dll", EntryPoint = "XInputGetState")] static extern uint GetState910(uint index, out XInputState state);

        static int dll;   // 0: xinput1_4, 1: xinput9_1_0, 2: none
        const short StickDeadZone = 16000;
        const byte TriggerThreshold = 100;

        static bool Get(uint index, out XInputState state)
        {
            state = new XInputState();
            while (dll < 2)
            {
                try { return (dll == 0 ? GetState14(index, out state) : GetState910(index, out state)) == 0; }
                catch (DllNotFoundException) { dll++; }
                catch (EntryPointNotFoundException) { dll++; }
            }
            return false;
        }

        public static int Read()
        {
            int held = 0;
            for (uint i = 0; i < 4; i++)
            {
                XInputState st;
                if (!Get(i, out st)) continue;
                held |= st.Buttons;
                if (st.LeftY > StickDeadZone) held |= Up;
                if (st.LeftY < -StickDeadZone) held |= Down;
                if (st.LeftTrigger > TriggerThreshold) held |= LT;
                if (st.RightTrigger > TriggerThreshold) held |= RT;
            }
            return held;
        }
    }

    static class Win
    {
        [DllImport("user32.dll")] public static extern bool ReleaseCapture();
        [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hWnd, int msg, IntPtr wParam, IntPtr lParam);
        public const int WM_NCHITTEST = 0x84, WM_NCLBUTTONDOWN = 0xA1, HTCAPTION = 2, HTTRANSPARENT = -1;

        public static Point MsgPoint(IntPtr lParam)
        {
            long v = lParam.ToInt64();
            return new Point((short)(v & 0xFFFF), (short)((v >> 16) & 0xFFFF));
        }

        // Near the window's edge? (child controls then let the window handle it, for resizing)
        public static bool NearEdge(Control c, IntPtr lParam)
        {
            var f = c.FindForm();
            if (f == null || f.WindowState != FormWindowState.Normal) return false;
            var p = f.PointToClient(MsgPoint(lParam));
            int e = Theme.S(6);
            return p.X < e || p.Y < e || p.X >= f.ClientSize.Width - e || p.Y >= f.ClientSize.Height - e;
        }
    }

    static class Res
    {
        public static Stream Open(string name) { return Assembly.GetExecutingAssembly().GetManifestResourceStream(name); }

        public static Image Image(string name)
        {
            using (var s = Open(name))
            {
                if (s == null) return null;
                using (var img = System.Drawing.Image.FromStream(s)) return new Bitmap(img);
            }
        }

        public static Icon AppIcon()
        {
            using (var s = Open("app.ico")) return s == null ? SystemIcons.Application : new Icon(s);
        }
    }

    // ------------------------------------------------------------------ unlock record

    class Store
    {
        public readonly Achievement[] All = Data.All;
        readonly Dictionary<int, long> deUnlocked = new Dictionary<int, long>();   // DE achievement number -> FILETIME
        readonly Dictionary<string, long> logRead = new Dictionary<string, long>();   // per log file: bytes already processed
        string profileLog = "";                                                           // player 1's profile's log (the list's)
        readonly Dictionary<string, long> otherRead = new Dictionary<string, long>();     // the other local players' logs: bytes read
        public bool Switched;                                                             // the last Poll found another profile

        public Store()
        {
            Poll(false);   // everything already in the log is old news: no pop-ups for it
        }

        public int Count { get { int n = 0; foreach (var a in All) if (a.Unlocked) n++; return n; } }
        public int Score { get { int n = 0; foreach (var a in All) if (a.Unlocked) n += a.Score; return n; } }
        public int MaxScore { get { int n = 0; foreach (var a in All) n += a.Score; return n; } }

        // The other local players' logs: their new achievements (pop-ups only; the list is player 1's).
        void ReadOther(string path, bool fresh, List<Achievement> got)
        {
            long done;
            bool known = otherRead.TryGetValue(path, out done);
            try
            {
                var fi = new FileInfo(path);
                if (!fi.Exists) { otherRead[path] = 0; return; }
                if (fi.Length < done) done = 0;
                if (fi.Length - done < 16) { otherRead[path] = done; return; }
                using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                {
                    fs.Seek(done, SeekOrigin.Begin);
                    var data = new byte[(fs.Length - done) / 16 * 16];
                    int off = 0;
                    while (off < data.Length) { int r = fs.Read(data, off, data.Length - off); if (r <= 0) break; off += r; }
                    off = off / 16 * 16;
                    otherRead[path] = done + off;
                    if (!known || !fresh) return;      // a log first seen: what it has is old news
                    for (int i = 0; i + 16 <= off; i += 16)
                    {
                        int id = BitConverter.ToInt32(data, i);
                        var a = Array.Find(All, delegate(Achievement x) { return Array.IndexOf(x.DeIds, id) >= 0; });
                        if (a != null && !got.Contains(a)) got.Add(a);
                    }
                }
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }

        // Reads new records from the log. Returns the achievements that are newly unlocked.
        void ReadLog(string path)
        {
            long done;
            logRead.TryGetValue(path, out done);
            byte[] data;
            try
            {
                var fi = new FileInfo(path);
                if (!fi.Exists) { logRead[path] = 0; return; }
                if (fi.Length < done) done = 0;               // log was cleared
                if (fi.Length - done < 16) { logRead[path] = done; return; }
                using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                {
                    fs.Seek(done, SeekOrigin.Begin);
                    long n = (fs.Length - done) / 16 * 16;
                    data = new byte[n];
                    int off = 0;
                    while (off < n) { int r = fs.Read(data, off, (int)n - off); if (r <= 0) break; off += r; }
                    if (off < n) Array.Resize(ref data, off / 16 * 16);
                }
            }
            catch (IOException) { return; }
            catch (UnauthorizedAccessException) { return; }

            for (int i = 0; i + 16 <= data.Length; i += 16)
            {
                int id = BitConverter.ToInt32(data, i);
                long ft = BitConverter.ToInt64(data, i + 8);
                if (ft == 0) ft = DateTime.UtcNow.ToFileTimeUtc();
                long had;
                if (!deUnlocked.TryGetValue(id, out had) || ft < had) deUnlocked[id] = ft;   // earliest record wins
            }
            logRead[path] = done + data.Length;
        }

        public List<Achievement> Poll(bool fresh)
        {
            var got = new List<Achievement>();
            var logs = Game.ProfileLogs();
            string first = logs != null ? logs[0] : "";
            Switched = false;
            if (first != profileLog)
            {
                // another profile for player 1: its own achievements, and nothing to pop up for them
                Switched = profileLog.Length > 0 || logRead.Count > 0;
                profileLog = first;
                deUnlocked.Clear(); logRead.Clear();
                foreach (var a in All) a.UnlockedAt = a.Unlocked360;
                fresh = false;
            }
            if (logs != null)
            {
                ReadLog(logs[0]);
                for (int i = 1; i < logs.Length; i++) if (logs[i] != logs[0]) ReadOther(logs[i], fresh, got);
            }
            else foreach (var path in Game.LogPaths) ReadLog(path);

            // An achievement unlocks once all of its Definitive Edition achievements have (time = the last of them).
            foreach (var a in All)
            {
                if (a.Unlocked || a.DeIds.Length == 0) continue;
                long when = 0;
                foreach (int d in a.DeIds)
                {
                    long t;
                    if (!deUnlocked.TryGetValue(d, out t)) { when = 0; break; }
                    if (t > when) when = t;
                }
                if (when == 0) continue;
                a.UnlockedAt = when;
                if (fresh) got.Add(a);
            }

            // Achievements made of other achievements on this list (Ready For The Sequel): unlocked once all of
            // them are, at the time of the last one. Repeated in case one of these depends on another.
            bool more = true;
            while (more)
            {
                more = false;
                foreach (var a in All)
                {
                    if (a.Unlocked || a.Requires.Length == 0) continue;
                    long when = 0;
                    foreach (int id in a.Requires)
                    {
                        Achievement r = Array.Find(All, delegate(Achievement x) { return x.Id == id; });
                        if (r == null || !r.Unlocked) { when = 0; break; }
                        if (r.UnlockedAt > when) when = r.UnlockedAt;
                    }
                    if (when == 0) continue;
                    a.UnlockedAt = when;
                    if (fresh) got.Add(a);
                    more = true;
                }
            }
            return got;
        }
    }

    // ------------------------------------------------------------------ theme (matches the launcher)

    static class Theme
    {
        public static readonly Color BgTop = Color.FromArgb(14, 28, 46);
        public static readonly Color BgBottom = Color.FromArgb(5, 10, 18);
        public static readonly Color Frame = Color.FromArgb(44, 92, 134);
        // the window's border, as the Halo PC installer's: 3 pixels of periwinkle
        public static readonly Color WindowBorder = Color.FromArgb(78, 119, 252);
        public static int WindowBorderWidth { get { return Math.Max(2, S(3)); } }
        public static readonly Color Accent = Color.FromArgb(92, 196, 255);
        public static readonly Color RowTop = Color.FromArgb(22, 46, 72);
        public static readonly Color RowBottom = Color.FromArgb(10, 24, 40);
        public static readonly Color Text = Color.FromArgb(232, 242, 255);
        public static readonly Color TextDim = Color.FromArgb(132, 162, 194);
        public static readonly Color Gold = Color.FromArgb(255, 214, 120);

        public static float Scale = 1f;
        public static int S(float v) { return (int)Math.Round(v * Scale); }
        // Bahnschrift (built into Windows 10 and 11); Segoe UI if it's missing.
        public static Font UiFont(float px, FontStyle st)
        {
            if ((st & FontStyle.Bold) != 0)
                return Pick(px, st & ~FontStyle.Bold, "Bahnschrift SemiBold", null, "Bahnschrift", FontStyle.Bold, "Segoe UI", st);
            return Pick(px, st, "Bahnschrift", null, null, st, "Segoe UI", st);
        }

        public static Font LightFont(float px)
        {
            return Pick(px, FontStyle.Regular, "Bahnschrift Light", null, "Bahnschrift", FontStyle.Regular, "Segoe UI Light", FontStyle.Regular);
        }

        static Font Pick(float px, FontStyle st, string first, string unused, string second, FontStyle st2, string last, FontStyle st3)
        {
            foreach (var pair in new[] { new KeyValuePair<string, FontStyle>(first, st), new KeyValuePair<string, FontStyle>(second, st2) })
            {
                if (pair.Key == null) continue;
                try
                {
                    var f = new Font(pair.Key, px * Scale, pair.Value, GraphicsUnit.Pixel);
                    if (string.Equals(f.Name, pair.Key, StringComparison.OrdinalIgnoreCase)) return f;
                    f.Dispose();
                }
                catch { }
            }
            return new Font(last, px * Scale, st3, GraphicsUnit.Pixel);
        }

        public static GraphicsPath Round(RectangleF r, float rad)
        {
            var p = new GraphicsPath();
            float d = rad * 2;
            if (rad <= 0) { p.AddRectangle(r); return p; }
            p.AddArc(r.X, r.Y, d, d, 180, 90);
            p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
            p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
            p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
            p.CloseFigure();
            return p;
        }

        // Gamerscore symbol (the Games for Windows - LIVE "GScore" icon, gscore.png). The colour only sets how
        // strongly it shows: faded with the text's alpha, and dimmer next to dim (locked) text.
        static Image gscore;

        // Exact text layout (no GDI+ padding), so the symbol sits a fixed gap after the digits.
        public static readonly StringFormat Typo = MakeTypo();
        static StringFormat MakeTypo()
        {
            var f = (StringFormat)StringFormat.GenericTypographic.Clone();
            f.FormatFlags |= StringFormatFlags.MeasureTrailingSpaces | StringFormatFlags.NoWrap;
            return f;
        }
        public static float TextW(Graphics g, string s, Font f) { return g.MeasureString(s, f, PointF.Empty, Typo).Width; }
        public static float GGap { get { return S(3); } }

        // Size of the symbol for text in this font, and the top for it to sit centred on the digits
        // drawn at y (digits are cap height - about 0.7 em in Bahnschrift and Segoe UI - standing on the baseline).
        public static float GSize(Font f) { return f.Size * 0.86f; }
        public static float GTop(Font f, float y)
        {
            var ff = f.FontFamily;
            float ascent = f.Size * ff.GetCellAscent(f.Style) / ff.GetEmHeight(f.Style);
            float digitsMid = y + ascent - f.Size * 0.35f;
            return digitsMid - GSize(f) / 2;
        }
        public static void GMark(Graphics g, RectangleF r, Color c)
        {
            if (gscore == null) gscore = Res.Image("gscore.png");
            if (gscore == null) return;
            float op = (c.A / 255f) * Math.Max(0.45f, (c.R + c.G + c.B) / 765f);
            using (var ia = new ImageAttributes())
            {
                var cm = new ColorMatrix();
                cm.Matrix33 = Math.Min(1f, op);
                ia.SetColorMatrix(cm);
                var old = g.InterpolationMode;
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                var d = new Rectangle((int)Math.Round(r.X), (int)Math.Round(r.Y), (int)Math.Round(r.Width), (int)Math.Round(r.Height));
                g.DrawImage(gscore, d, 0, 0, gscore.Width, gscore.Height, GraphicsUnit.Pixel, ia);
                g.InterpolationMode = old;
            }
        }

        // Generic achievement medal, for achievements without an icon (and the pop-up's badge).
        public static void Medal(Graphics g, RectangleF r, bool lit)
        {
            Color a = lit ? Accent : Color.FromArgb(70, 96, 120);
            using (var b = new LinearGradientBrush(r, lit ? Color.FromArgb(30, 78, 118) : Color.FromArgb(26, 36, 48),
                                                       lit ? Color.FromArgb(8, 28, 48) : Color.FromArgb(12, 18, 26), LinearGradientMode.Vertical))
            using (var path = Round(r, r.Width * 0.12f)) g.FillPath(b, path);
            float cx = r.X + r.Width / 2, cy = r.Y + r.Height * 0.47f, R = r.Width * 0.30f;
            var star = new PointF[10];
            for (int i = 0; i < 10; i++)
            {
                double ang = -Math.PI / 2 + i * Math.PI / 5;
                float rad = (i % 2 == 0) ? R : R * 0.45f;
                star[i] = new PointF(cx + (float)(Math.Cos(ang) * rad), cy + (float)(Math.Sin(ang) * rad));
            }
            using (var b = new SolidBrush(a)) g.FillPolygon(b, star);
            using (var pen = new Pen(Color.FromArgb(lit ? 150 : 70, a), Math.Max(1f, r.Width * 0.04f)))
                g.DrawEllipse(pen, cx - R * 1.35f, cy - R * 1.35f, R * 2.7f, R * 2.7f);
        }
    }

    // ------------------------------------------------------------------ icons

    static class Icons
    {
        static readonly Dictionary<int, Image> cache = new Dictionary<int, Image>();
        static readonly Dictionary<int, Image> grey = new Dictionary<int, Image>();

        // "Achievement Icons" folders searched for your own pictures, named after the achievement.
        static IEnumerable<string> Folders()
        {
            yield return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "Achievement Icons");
            yield return Path.Combine(Game.Root, "Achievement Icons");
        }

        static Image Load(Achievement a)
        {
            // Your own picture first ("Achievement Icons\<name>.png/.jpg"), then the built-in 360 icon.
            foreach (var dir in Folders())
                foreach (var ext in new[] { ".png", ".jpg", ".jpeg", ".bmp", ".gif" })
                {
                    string p = Path.Combine(dir, a.FileName + ext);
                    if (!File.Exists(p)) continue;
                    try { using (var s = new MemoryStream(File.ReadAllBytes(p))) using (var img = Image.FromStream(s)) return new Bitmap(img); }
                    catch { }
                }
            var built = Res.Image("icons." + a.Id + ".png");
            if (built != null) return built;
            // Fallback: the source "icons" folder (<id>.png) next to this program.
            string f = Path.Combine(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "icons"), a.Id + ".png");
            try { if (File.Exists(f)) using (var s = new MemoryStream(File.ReadAllBytes(f))) using (var img = Image.FromStream(s)) return new Bitmap(img); }
            catch { }
            return null;
        }

        public static Image Get(Achievement a, bool unlocked)
        {
            int id = a.Id;
            Image img;
            if (!cache.TryGetValue(id, out img)) { img = Load(a); cache[id] = img; }
            if (img == null || unlocked) return img;
            Image g;
            if (!grey.TryGetValue(id, out g)) { g = Greyed(img); grey[id] = g; }
            return g;
        }

        static Image Greyed(Image src)
        {
            var bmp = new Bitmap(src.Width, src.Height, PixelFormat.Format32bppArgb);
            var cm = new ColorMatrix(new float[][] {
                new float[] { 0.21f, 0.21f, 0.21f, 0, 0 },     // locked: the same picture in grey, a little darker
                new float[] { 0.41f, 0.41f, 0.41f, 0, 0 },
                new float[] { 0.08f, 0.08f, 0.08f, 0, 0 },
                new float[] { 0, 0, 0, 1, 0 },
                new float[] { 0.03f, 0.04f, 0.06f, 0, 1 } });
            using (var g = Graphics.FromImage(bmp))
            using (var ia = new ImageAttributes())
            {
                ia.SetColorMatrix(cm);
                g.DrawImage(src, new Rectangle(0, 0, bmp.Width, bmp.Height), 0, 0, src.Width, src.Height, GraphicsUnit.Pixel, ia);
            }
            return bmp;
        }

        public static void Draw(Graphics g, Achievement a, Rectangle r)
        {
            var img = Get(a, a.Unlocked);
            if (img != null)
            {
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                using (var path = Theme.Round(r, r.Width * 0.08f))
                {
                    var clip = g.Clip; g.SetClip(path, CombineMode.Intersect);
                    g.DrawImage(img, r);
                    g.Clip = clip;
                }
            }
            else Theme.Medal(g, r, a.Unlocked);
            if (!a.Unlocked) Lock(g, r);
        }

        static void Lock(Graphics g, Rectangle r)
        {
            float w = r.Width * 0.30f, h = w * 0.8f;
            float x = r.Right - w - r.Width * 0.06f, y = r.Bottom - h - r.Height * 0.06f;
            using (var pen = new Pen(Theme.TextDim, Math.Max(1.4f, w * 0.13f)))
                g.DrawArc(pen, x + w * 0.2f, y - h * 0.55f, w * 0.6f, h * 0.9f, 180, 180);
            using (var b = new SolidBrush(Theme.TextDim)) using (var p = Theme.Round(new RectangleF(x, y, w, h), w * 0.12f)) g.FillPath(b, p);
        }
    }

    // ------------------------------------------------------------------ the background part: tray, log watching, pop-ups

    class Handler : ApplicationContext
    {
        readonly Store store = new Store();
        readonly NotifyIcon tray = new NotifyIcon();
        readonly System.Windows.Forms.Timer poll = new System.Windows.Forms.Timer();
        readonly Queue<Achievement> pending = new Queue<Achievement>();
        readonly Control ui = new Control();     // for marshalling the "show" signal onto this thread
        readonly EventWaitHandle showEvent;
        ListForm list;
        Toast toast;
        bool sawGame;
        DateTime started = DateTime.Now, lastGame = DateTime.Now;

        public Handler()
        {
            GC.KeepAlive(ui.Handle);   // creates the window handle used for BeginInvoke
            using (var g = ui.CreateGraphics()) Theme.Scale = Math.Max(1f, g.DpiX / 96f);

            tray.Icon = Res.AppIcon();
            tray.Text = "OpenCE Achievements";
            var menu = new ContextMenuStrip();
            menu.Items.Add("Achievements", null, delegate { ShowList(); });
            menu.Items.Add("Exit", null, delegate { ExitThread(); });
            tray.ContextMenuStrip = menu;
            tray.DoubleClick += delegate { ShowList(); };
            tray.Visible = true;

            showEvent = new EventWaitHandle(false, EventResetMode.AutoReset, Program.ShowEventName);
            var t = new Thread(delegate()
            {
                while (true)
                {
                    try { showEvent.WaitOne(); } catch { return; }
                    try { ui.BeginInvoke((MethodInvoker)ShowList); } catch { return; }
                }
            });
            t.IsBackground = true;
            t.Start();

            poll.Interval = 400;
            poll.Tick += delegate { Tick(); };
            poll.Start();

            if (!Program.Background) ShowList();
        }

        string shownTag;

        void Tick()
        {
            // the profile in play changed in the game: the list's header shows it
            string tag = Game.Gamertag();
            if (tag != shownTag)
            {
                shownTag = tag;
                if (list != null && !list.IsDisposed) list.RefreshHeader(store);
            }
            foreach (var a in store.Poll(true)) pending.Enqueue(a);
            // player 1 on another profile: the list is that profile's
            if (store.Switched && list != null && !list.IsDisposed) list.Refresh(store);
            if (pending.Count > 0 && (toast == null || toast.IsDisposed))
            {
                toast = new Toast(pending.Dequeue());
                toast.Show();
                if (list != null && !list.IsDisposed) list.Refresh(store);
            }

            // In the background, close once the game has been shut for a while (nothing left to watch).
            if ((DateTime.Now - started).TotalSeconds > 5 && DateTime.Now.Second % 5 == 0)
            {
                bool running = Game.Running();
                if (running) { sawGame = true; lastGame = DateTime.Now; }
                bool listOpen = list != null && !list.IsDisposed && list.Visible;
                bool toastUp = toast != null && !toast.IsDisposed;
                if (!running && !listOpen && !toastUp && pending.Count == 0)
                {
                    double idle = (DateTime.Now - lastGame).TotalSeconds;
                    if ((sawGame && idle > 20) || (!sawGame && idle > 180)) ExitThread();
                }
            }
        }

        void ShowList()
        {
            if (list == null || list.IsDisposed) list = new ListForm(store);
            list.Refresh(store);
            list.Show();
            if (list.WindowState == FormWindowState.Minimized) list.WindowState = FormWindowState.Normal;
            list.Activate();
            list.BringToFront();
        }

        protected override void ExitThreadCore()
        {
            tray.Visible = false;
            tray.Dispose();
            poll.Stop();
            if (list != null && !list.IsDisposed) { list.Quitting = true; list.Close(); }
            if (toast != null && !toast.IsDisposed) toast.Close();
            base.ExitThreadCore();
        }
    }

    // ------------------------------------------------------------------ the pop-up (per-pixel alpha, never takes focus)

    class Toast : Form
    {
        [DllImport("user32.dll")] static extern bool UpdateLayeredWindow(IntPtr hwnd, IntPtr hdcDst, ref POINT pptDst, ref SIZE psize, IntPtr hdcSrc, ref POINT pprSrc, int crKey, ref BLENDFUNCTION pblend, int dwFlags);
        [DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr hWnd);
        [DllImport("user32.dll")] static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);
        [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleDC(IntPtr hDC);
        [DllImport("gdi32.dll")] static extern bool DeleteDC(IntPtr hdc);
        [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr hDC, IntPtr hObject);
        [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr hObject);
        [DllImport("winmm.dll", CharSet = CharSet.Unicode)] static extern int mciSendString(string command, StringBuilder ret, int retLen, IntPtr hwnd);
        [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

        [StructLayout(LayoutKind.Sequential)] struct POINT { public int x, y; public POINT(int x, int y) { this.x = x; this.y = y; } }
        [StructLayout(LayoutKind.Sequential)] struct SIZE { public int cx, cy; public SIZE(int cx, int cy) { this.cx = cx; this.cy = cy; } }
        [StructLayout(LayoutKind.Sequential)] struct RECT { public int l, t, r, b; }
        [StructLayout(LayoutKind.Sequential, Pack = 1)] struct BLENDFUNCTION { public byte op, flags, alpha, format; }

        const int WS_EX_LAYERED = 0x80000, WS_EX_TRANSPARENT = 0x20, WS_EX_TOOLWINDOW = 0x80, WS_EX_TOPMOST = 0x8, WS_EX_NOACTIVATE = 0x08000000;

        readonly Achievement ach;
        readonly System.Windows.Forms.Timer anim = new System.Windows.Forms.Timer();
        readonly Stopwatch clock = new Stopwatch();
        readonly Font line1, line2;
        readonly string text2;
        readonly int W, H;
        readonly Bitmap bar, globeBar; // the finished bar at full width (trophy / Xbox globe badge), built once
        readonly int ringW, capW;     // the round badge on the left, the rounded end on the right
        Point origin;
        const double OpenMs = 450, HoldMs = 5200, CloseMs = 450;
        // The badge opens on the Xbox globe, then fades between the globe and the trophy 5 times (0.7 s apart),
        // settling on the trophy at about 4 s.
        const double FlickerAt = 900, FlickerStep = 700, FadeMs = 350;
        const int Swaps = 5;
        // soft white glow just outside the bar's edge
        const int GlowLayers = 4, GlowLayerAlpha = 18;
        readonly int G;               // room around the bar for the glow

        // toast.png (the Xbox 360 bar with the trophy badge), in units of its height:
        const float SrcRing = 1.02f;       // the bar's left end up to the right of the badge ring
        const float SrcUniformFrom = 0.43f, SrcUniformTo = 0.893f;   // plain bar body, as fractions of the width
        // text layout from the Xbox 360 toast, in units of the bar height
        const float TextX = 1.21f, Base1 = 0.401f, Base2 = 0.784f, Em1 = 0.307f, Em2 = 0.233f, PadRight = 0.70f;

        public Toast(Achievement a)
        {
            ach = a;
            FormBorderStyle = FormBorderStyle.None;
            ShowInTaskbar = false;
            StartPosition = FormStartPosition.Manual;
            H = Theme.S(80);
            line1 = SegoeUi(H * Em1);
            line2 = SegoeUi(H * Em2);
            text2 = a.Score + "G - " + a.Name;
            using (var bmp = new Bitmap(1, 1)) using (var g = Graphics.FromImage(bmp))
            {
                float tw = Math.Max(Theme.TextW(g, "Achievement unlocked", line1), Theme.TextW(g, text2, line2));
                W = (int)Math.Ceiling(Math.Min(Theme.S(1100), H * (TextX + PadRight) + tw));
            }
            capW = (int)Math.Round(H * 0.58f);
            ringW = (int)Math.Round(H * SrcRing);
            W = Math.Max(W, ringW + capW + Theme.S(40));
            bar = BuildBar("toast.png");
            globeBar = BuildBar("toast_globe.png");
            G = Math.Max(GlowLayers, Theme.S(GlowLayers)) + 1;
            Size = new Size(W + 2 * G, H + 2 * G);
            anim.Interval = 15;
            anim.Tick += delegate { Step(); };
        }

        static Font SegoeUi(float px)
        {
            try
            {
                var f = new Font("Segoe UI", px, FontStyle.Regular, GraphicsUnit.Pixel);
                if (f.Name == "Segoe UI") return f;
                f.Dispose();
            }
            catch { }
            return new Font(SystemFonts.MessageBoxFont.FontFamily, px, FontStyle.Regular, GraphicsUnit.Pixel);
        }

        // Left part of the bar image (badge + bar start), the plain body stretched, and its rounded right end.
        Bitmap BuildBar(string image)
        {
            var b = new Bitmap(W, H, PixelFormat.Format32bppArgb);
            using (var src = Res.Image(image))
            using (var g = Graphics.FromImage(b))
            using (var ia = new ImageAttributes())
            {
                g.Clear(Color.Transparent);
                if (src == null) return b;
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                ia.SetWrapMode(WrapMode.TileFlipXY);   // no soft edges where the pieces meet
                float sc = (float)H / src.Height;
                int u0 = (int)(src.Width * SrcUniformFrom), u1 = (int)(src.Width * SrcUniformTo);
                int leftW = Math.Min((int)Math.Round(u0 * sc), W - capW);
                int rightSrc = src.Width - u1;
                // body: one plain column stretched between the two ends (not under their transparent corners)
                g.DrawImage(src, new Rectangle(leftW - 1, 0, W - capW - leftW + 2, H), (u0 + u1) / 2, 0, 2, src.Height, GraphicsUnit.Pixel, ia);
                // left: badge and bar start
                g.DrawImage(src, new Rectangle(0, 0, leftW, H), 0, 0, leftW / sc, src.Height, GraphicsUnit.Pixel, ia);
                // right: rounded end
                g.DrawImage(src, new Rectangle(W - capW, 0, capW, H), src.Width - rightSrc, 0, rightSrc, src.Height, GraphicsUnit.Pixel, ia);
            }
            return b;
        }

        protected override bool ShowWithoutActivation { get { return true; } }

        protected override CreateParams CreateParams
        {
            get
            {
                var cp = base.CreateParams;
                cp.ExStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE;
                return cp;
            }
        }

        protected override void OnShown(EventArgs e)
        {
            base.OnShown(e);
            // Bottom centre of the screen the game (or whatever is in front) is on, like the 360 notification.
            Screen scr = Screen.PrimaryScreen;
            RECT rc;
            if (GetWindowRect(GetForegroundWindow(), out rc)) scr = Screen.FromRectangle(Rectangle.FromLTRB(rc.l, rc.t, rc.r, rc.b));
            var b = scr.Bounds;
            origin = new Point(b.X + (b.Width - Width) / 2, b.Bottom - Height - (int)(b.Height * 0.11));
            Location = origin;
            Sound();
            clock.Start();
            anim.Start();
            Step();
        }

        void Sound()
        {
            string f = Game.SoundFile();
            if (f == null) return;
            try
            {
                mciSendString("close hwach", null, 0, IntPtr.Zero);
                if (mciSendString("open \"" + f + "\" type mpegvideo alias hwach", null, 0, IntPtr.Zero) == 0)
                    mciSendString("play hwach", null, 0, IntPtr.Zero);
            }
            catch { }
        }

        // 0 = globe, 1 = trophy; each swap fades from one to the other.
        static double TrophyAmount(double ms)
        {
            if (ms < FlickerAt) return 0;
            int i = Math.Min(Swaps, (int)((ms - FlickerAt) / FlickerStep) + 1);   // latest swap so far
            double from = (i - 1) % 2, to = i % 2;
            double t = Math.Min(1, (ms - FlickerAt - (i - 1) * FlickerStep) / FadeMs);
            t = t * t * (3 - 2 * t);   // smooth
            return from + (to - from) * t;
        }

        static double Ease(double t) { t = Math.Max(0, Math.Min(1, t)); return 1 - Math.Pow(1 - t, 3); }

        void Step()
        {
            double ms = clock.Elapsed.TotalMilliseconds;
            double open, alpha;
            if (ms < OpenMs) { open = Ease(ms / OpenMs); alpha = Math.Min(1, ms / 180); }
            else if (ms < OpenMs + HoldMs) { open = 1; alpha = 1; }
            else if (ms < OpenMs + HoldMs + CloseMs) { double t = (ms - OpenMs - HoldMs) / CloseMs; open = 1 - Ease(t); alpha = 1 - Ease(t); }
            else { anim.Stop(); Close(); return; }
            Render(open, alpha, TrophyAmount(ms));
        }

        // The bar grows out of the round badge (like the 360), then shrinks back into it.
        void Render(double open, double alpha, double trophy)
        {
            using (var bmp = new Bitmap(W + 2 * G, H + 2 * G, PixelFormat.Format32bppArgb))
            {
                using (var g = Graphics.FromImage(bmp))
                {
                    g.Clear(Color.Transparent);
                    int minW = ringW + capW;
                    int barW = (int)Math.Round(minW + (W - minW) * open);
                    int x0 = G + (W - barW) / 2, y0 = G;

                    // glow: stacked faint pills just outside the bar's outline (the bar covers the inside)
                    g.SmoothingMode = SmoothingMode.AntiAlias;
                    for (int i = G - 1; i >= 1; i--)
                        using (var path = Theme.Round(new RectangleF(x0 - i + 1, y0 - i + 1, barW + 2 * i - 2, H + 2 * i - 2), (H + 2 * i - 2) / 2f))
                        using (var b = new SolidBrush(Color.FromArgb(GlowLayerAlpha, 255, 255, 255)))
                            g.FillPath(b, path);
                    g.SmoothingMode = SmoothingMode.None;

                    // the bar: globe version, with the trophy badge faded in over it
                    g.InterpolationMode = InterpolationMode.NearestNeighbor;
                    g.PixelOffsetMode = PixelOffsetMode.Half;
                    Bitmap baseBar = trophy >= 1 ? bar : globeBar;
                    int body = barW - capW;
                    g.DrawImage(baseBar, new Rectangle(x0, y0, body, H), new Rectangle(0, 0, body, H), GraphicsUnit.Pixel);
                    g.DrawImage(baseBar, new Rectangle(x0 + body, y0, capW, H), new Rectangle(W - capW, 0, capW, H), GraphicsUnit.Pixel);
                    if (trophy > 0 && trophy < 1)
                        using (var ia = new ImageAttributes())
                        {
                            ia.SetColorMatrix(new ColorMatrix { Matrix33 = (float)trophy });
                            g.DrawImage(bar, new Rectangle(x0, y0, ringW, H), 0, 0, ringW, H, GraphicsUnit.Pixel, ia);
                        }

                    // text, revealed as the bar opens
                    if (open > 0.35)
                    {
                        g.TextRenderingHint = TextRenderingHint.AntiAlias;
                        int ta = (int)(255 * Math.Min(1, (open - 0.35) / 0.4));
                        float tx = x0 + H * TextX;
                        g.SetClip(new RectangleF(tx, y0, Math.Max(0, x0 + barW - capW * 0.5f - tx), H));
                        using (var b1 = new SolidBrush(Color.FromArgb(ta, 245, 245, 245)))
                        {
                            g.DrawString("Achievement unlocked", line1, b1, tx, y0 + H * Base1 - Ascent(line1), Theme.Typo);
                            g.DrawString(text2, line2, b1, tx, y0 + H * Base2 - Ascent(line2), Theme.Typo);
                        }
                        g.ResetClip();
                    }
                }
                Push(bmp, (byte)(255 * alpha));
            }
        }

        static float Ascent(Font f)
        {
            var ff = f.FontFamily;
            return f.Size * ff.GetCellAscent(f.Style) / ff.GetEmHeight(f.Style);
        }

        void Push(Bitmap bmp, byte alpha)
        {
            IntPtr screen = GetDC(IntPtr.Zero), mem = CreateCompatibleDC(screen), hb = IntPtr.Zero, old = IntPtr.Zero;
            try
            {
                hb = bmp.GetHbitmap(Color.FromArgb(0));
                old = SelectObject(mem, hb);
                var size = new SIZE(bmp.Width, bmp.Height);
                var src = new POINT(0, 0);
                var dst = new POINT(origin.X, origin.Y);
                var blend = new BLENDFUNCTION { op = 0, flags = 0, alpha = alpha, format = 1 };
                UpdateLayeredWindow(Handle, screen, ref dst, ref size, mem, ref src, 0, ref blend, 2);
            }
            finally
            {
                if (old != IntPtr.Zero) SelectObject(mem, old);
                if (hb != IntPtr.Zero) DeleteObject(hb);
                DeleteDC(mem);
                ReleaseDC(IntPtr.Zero, screen);
            }
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing) { anim.Dispose(); line1.Dispose(); line2.Dispose(); bar.Dispose(); globeBar.Dispose(); }
            base.Dispose(disposing);
        }
    }

    // ------------------------------------------------------------------ the achievement list

    class ListForm : Form
    {
        readonly AchList listView;
        public bool Quitting;
        readonly Header header;

        public ListForm(Store store)
        {
            Text = "Halo: Combat Evolved - Achievements";
            Icon = Res.AppIcon();
            Cursor = GameCursor.Arrow;                   // assets\cursor.cur if built in, else the arrow
            FormBorderStyle = FormBorderStyle.None;      // own minimise / close buttons (the Halo PC installer's)
            MaximizeBox = false;                         // no maximise (no button, no snap-to-maximise, no double-click)
            Padding = new Padding(Theme.WindowBorderWidth);  // the installer's border: the form's colour around its children
            BackColor = Theme.WindowBorder;
            ForeColor = Theme.Text;
            StartPosition = FormStartPosition.CenterScreen;
            ClientSize = new Size(Theme.S(820), Theme.S(720));
            MinimumSize = new Size(Theme.S(720), Theme.S(420));
            KeyPreview = true;

            header = new Header();
            header.Dock = DockStyle.Top;
            header.Height = Theme.S(150);
            listView = new AchList();
            listView.Dock = DockStyle.Fill;
            Controls.Add(listView);
            Controls.Add(header);
            header.SortChanged += delegate { listView.SortMode = header.Sort; Sfx.Filter(); };
            header.FilterChanged += delegate { listView.Filter = header.Show; Sfx.Filter(); };
            KeyDown += delegate(object s, KeyEventArgs e)
            {
                if (e.KeyCode == Keys.Escape) Hide();
                else if (e.KeyCode == Keys.Left) { header.CycleSort(-1); e.Handled = true; }
                else if (e.KeyCode == Keys.Right) { header.CycleSort(1); e.Handled = true; }
            };
            pad.Interval = 30;
            pad.Tick += delegate { PadTick(); };
            pad.Start();
            Refresh(store);
        }

        // ---- the controller: up and down (D-pad or left stick) move the selection, the triggers a page,
        // left and right (D-pad) or Y the order, the bumpers the filter, X the header's picture, B or Back
        // closes the list
        readonly System.Windows.Forms.Timer pad = new System.Windows.Forms.Timer();
        int padHeld;
        DateTime padRepeat;

        void PadTick()
        {
            if (!Visible || WindowState == FormWindowState.Minimized || Form.ActiveForm != this) { padHeld = Pad.Read(); return; }
            int held = Pad.Read(), pressed = held & ~padHeld;
            const int Repeating = Pad.Up | Pad.Down | Pad.LT | Pad.RT;
            if ((held & Repeating) != 0 && (held & Repeating) == (padHeld & Repeating) && DateTime.Now >= padRepeat)
            {
                pressed |= held & Repeating;
                padRepeat = DateTime.Now.AddMilliseconds(90);
            }
            else if ((pressed & Repeating) != 0) padRepeat = DateTime.Now.AddMilliseconds(380);
            padHeld = held;
            if (pressed == 0) return;
            if ((pressed & (Pad.B | Pad.Back)) != 0) { Hide(); return; }   // (Hide plays the close sound)
            if ((pressed & Pad.Up) != 0) listView.MoveSelection(-1, false);
            if ((pressed & Pad.Down) != 0) listView.MoveSelection(1, false);
            if ((pressed & Pad.LT) != 0) listView.MoveSelection(-1, true);
            if ((pressed & Pad.RT) != 0) listView.MoveSelection(1, true);
            if ((pressed & Pad.Left) != 0) header.CycleSort(-1);
            if ((pressed & (Pad.Right | Pad.Y)) != 0) header.CycleSort(1);
            if ((pressed & Pad.LB) != 0) header.CycleShow(-1);
            if ((pressed & Pad.RB) != 0) header.CycleShow(1);
            if ((pressed & Pad.X) != 0) header.CyclePic(1);
        }

        public void Refresh(Store store)
        {
            header.Set(store);
            listView.Set(store.All);
        }

        // the header alone (the profile's name), the list as it is
        public void RefreshHeader(Store store) { header.Set(store); }

        protected override CreateParams CreateParams
        {
            get
            {
                var cp = base.CreateParams;
                cp.Style |= 0x00020000 | 0x00080000;     // WS_MINIMIZEBOX | WS_SYSMENU: taskbar click minimises / restores
                return cp;
            }
        }

        protected override void WndProc(ref Message m)
        {
            base.WndProc(ref m);
            if (m.Msg != Win.WM_NCHITTEST || WindowState != FormWindowState.Normal) return;
            var p = PointToClient(Win.MsgPoint(m.LParam));
            int e = Theme.S(6);
            bool l = p.X < e, r = p.X >= ClientSize.Width - e, t = p.Y < e, b = p.Y >= ClientSize.Height - e;
            int ht = t && l ? 13 : t && r ? 14 : b && l ? 16 : b && r ? 17 : l ? 10 : r ? 11 : t ? 12 : b ? 15 : 0;
            if (ht != 0) m.Result = (IntPtr)ht;
        }

        protected override bool ProcessDialogKey(Keys keyData)
        {
            // (Tab and the arrows go to the list's own handling, not to moving the focus)
            if ((keyData & Keys.KeyCode) == Keys.Tab) { header.CycleShow((keyData & Keys.Shift) != 0 ? -1 : 1); return true; }
            return base.ProcessDialogKey(keyData);
        }

        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            // Closing the window only hides it while the pop-up watcher is running.
            if (!Quitting && e.CloseReason == CloseReason.UserClosing) { e.Cancel = true; Hide(); }
            base.OnFormClosing(e);
        }

        // The open / close sound, once each time the window appears or goes away (tray, Escape, B / Back,
        // the close button, Alt+F4) - not when it is only brought to the front, and not as the program quits.
        bool soundShown;
        protected override void OnVisibleChanged(EventArgs e)
        {
            base.OnVisibleChanged(e);
            if (Visible == soundShown) return;
            soundShown = Visible;
            if (!Quitting) Sfx.Window();
        }
    }

    enum SortBy { Game, Date, Score }

    enum ShowFilter { All, Unlocked, Locked }

    class Header : Control
    {
        public event EventHandler SortChanged;
        public SortBy Sort = SortBy.Game;
        public event EventHandler FilterChanged;
        public ShowFilter Show = ShowFilter.All;
        string tag = "Achievements";
        Image pic;
        static Image tile;
        // the pictures to choose from: assets\gamerpic_0.png on (built in; the first the Halo logo), the arrows on
        // the picture's sides, or X on the controller, cycle them; the choice is the profile's
        static List<Image> pics;
        int picIndex;
        int arrowHot = -1;                  // 0 = left, 1 = right
        int count, total, score, max;
        readonly Font tagFont = Theme.LightFont(30f), infoFont = Theme.UiFont(15f, FontStyle.Regular), tabFont = Theme.UiFont(13.5f, FontStyle.Bold);
        // 0-2: order (left), 3-5: which achievements to show (right)
        readonly Rectangle[] tabs = new Rectangle[6];
        static readonly string[] TabNames = { "ALL", "BY DATE", "BY GAMERSCORE", "SHOW ALL", "UNLOCKED", "LOCKED" };
        int hot = -1;

        protected override void WndProc(ref Message m)
        {
            if (m.Msg == Win.WM_NCHITTEST && Win.NearEdge(this, m.LParam)) { m.Result = (IntPtr)Win.HTTRANSPARENT; return; }
            base.WndProc(ref m);
        }

        public Header()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.UserPaint | ControlStyles.ResizeRedraw, true);
            Cursor = GameCursor.Arrow;
        }

        // the next or previous order, or filter (keyboard and controller)
        public void CycleSort(int step)
        {
            Sort = (SortBy)(((int)Sort + step + 3) % 3); Invalidate();
            if (SortChanged != null) SortChanged(this, EventArgs.Empty);
        }

        public void CycleShow(int step)
        {
            Show = (ShowFilter)(((int)Show + step + 3) % 3); Invalidate();
            if (FilterChanged != null) FilterChanged(this, EventArgs.Empty);
        }

        static List<Image> Pics()
        {
            if (pics == null)
            {
                pics = new List<Image>();
                for (int i = 0; i < 64; i++)
                {
                    var im = Res.Image("gamerpic_" + i + ".png");
                    if (im == null) break;
                    pics.Add(im);
                }
            }
            return pics;
        }

        // the next or previous picture (the arrows, X), kept for the profile
        public void CyclePic(int step)
        {
            int n = Pics().Count;
            if (n < 2) return;
            picIndex = ((picIndex + step) % n + n) % n;
            Game.SaveGamerpicChoice(picIndex);
            Sfx.Filter();
            Invalidate();
        }

        Rectangle PicRect() { int m = Theme.S(22), ps = Theme.S(96); return new Rectangle(m, m, ps, ps); }

        // the arrows, small, beside the picture's left and right sides (in the gaps before the window's edge
        // and the name, clear of the window's resizing edge)
        Rectangle ArrowRect(int i)
        {
            var pr = PicRect();
            int w = Theme.S(14), h = Theme.S(30);
            return new Rectangle(i == 0 ? pr.X - Theme.S(2) - w : pr.Right + Theme.S(3), pr.Y + (pr.Height - h) / 2, w, h);
        }

        int ArrowAt(Point p)
        {
            if (Pics().Count < 2) return -1;
            for (int i = 0; i < 2; i++) if (ArrowRect(i).Contains(p)) return i;
            return -1;
        }

        void DrawArrows(Graphics g)
        {
            if (Pics().Count < 2) return;
            for (int i = 0; i < 2; i++)
            {
                var r = ArrowRect(i);
                bool hotArrow = arrowHot == i;
                if (hotArrow)
                    using (var path = Theme.Round(r, Theme.S(4)))
                    using (var b = new SolidBrush(Color.FromArgb(150, 6, 14, 26)))
                        g.FillPath(b, path);
                float cx = r.X + r.Width / 2f, cy = r.Y + r.Height / 2f, aw = Theme.S(5), ah = Theme.S(8);
                var pts = i == 0
                    ? new[] { new PointF(cx + aw / 2, cy - ah), new PointF(cx - aw / 2, cy), new PointF(cx + aw / 2, cy + ah) }
                    : new[] { new PointF(cx - aw / 2, cy - ah), new PointF(cx + aw / 2, cy), new PointF(cx - aw / 2, cy + ah) };
                using (var pen = new Pen(hotArrow ? Color.White : Color.FromArgb(150, 175, 210), Math.Max(2f, Theme.S(2))))
                {
                    pen.StartCap = pen.EndCap = LineCap.Round; pen.LineJoin = LineJoin.Round;
                    g.DrawLines(pen, pts);
                }
            }
        }

        public void Set(Store s)
        {
            tag = Game.Gamertag();
            if (pic != null) pic.Dispose();
            pic = Game.Gamerpic();
            // (the profile's picture, of the built-in ones)
            picIndex = Pics().Count > 0 ? Math.Min(Game.GamerpicChoice(), Pics().Count - 1) : 0;
            count = s.Count; total = s.All.Length; score = s.Score; max = s.MaxScore;
            Invalidate();
        }

        // Minimise / close: the Halo PC installer's caption buttons, 24x24 slanted tiles. Each is a strip of six
        // states, one under another (assets\win_minimize.png and win_close.png, 24x144): normal, hover, pressed
        // are the first three. Without them the same tiles are drawn.
        static Image minStrip, closeStrip;
        static bool stripsLoaded;
        int btnHot = -1, btnDown = -1;      // 0 = minimise, 1 = close
        const int BtnSize = 24, BtnGap = 3, BtnMargin = 6;

        Rectangle BtnRect(int i)
        {
            int s = Theme.S(BtnSize), x = Width - Theme.S(BtnMargin) - s, y = Theme.S(BtnMargin);
            if (i == 0) x -= s + Theme.S(BtnGap);
            return new Rectangle(x, y, s, s);
        }

        int BtnAt(Point p)
        {
            for (int i = 1; i >= 0; i--) if (BtnRect(i).Contains(p)) return i;
            return -1;
        }

        // the installer's tile drawn, for want of its picture: a slanted box outlined in its blue, the
        // minimise bar or the close cross in it
        static void DrawTile(Graphics g, Rectangle r, int which, int state)
        {
            float k = r.Width / 24f;
            Func<float, float, PointF> P = delegate(float x, float y) { return new PointF(r.X + x * k, r.Y + y * k); };
            var outline = new[] { P(5, 2.5f), P(22, 2.5f), P(19, 21.5f), P(2, 21.5f) };
            Color fill = state == 2 ? Color.FromArgb(4, 14, 24) : state == 1 ? Color.FromArgb(21, 75, 128) : Color.FromArgb(8, 30, 51);
            Color edge = Color.FromArgb(41, 150, 255);
            g.SmoothingMode = SmoothingMode.AntiAlias;
            using (var b = new SolidBrush(fill)) g.FillPolygon(b, outline);
            if (state == 2)
                using (var path = new GraphicsPath())
                {
                    path.AddPolygon(outline);
                    using (var glow = new PathGradientBrush(path))
                    {
                        glow.CenterColor = Color.FromArgb(150, 41, 150, 255);
                        glow.SurroundColors = new[] { Color.FromArgb(0, 41, 150, 255) };
                        g.FillPath(glow, path);
                    }
                }
            using (var pen = new Pen(edge, Math.Max(1.5f, 2f * k))) g.DrawPolygon(pen, outline);
            using (var pen = new Pen(edge, Math.Max(1.5f, 2f * k)))
            {
                if (which == 0) g.DrawLine(pen, P(8, 17), P(14, 17));
                else { g.DrawLine(pen, P(6.5f, 5.5f), P(17.5f, 18.5f)); g.DrawLine(pen, P(19.5f, 5.5f), P(4.5f, 18.5f)); }
            }
        }

        void DrawButtons(Graphics g)
        {
            if (!stripsLoaded)
            {
                stripsLoaded = true;
                minStrip = Res.Image("win_minimize.png");
                closeStrip = Res.Image("win_close.png");
            }
            for (int i = 0; i < 2; i++)
            {
                int st = btnHot == i ? (btnDown == i ? 2 : 1) : 0;
                var r = BtnRect(i);
                var strip = i == 0 ? minStrip : closeStrip;
                if (strip != null && strip.Width > 0 && strip.Height >= strip.Width * 3)
                {
                    int cell = strip.Width;
                    // (whole multiples kept sharp, as the installer's pixels are)
                    g.InterpolationMode = r.Width % cell == 0 ? InterpolationMode.NearestNeighbor : InterpolationMode.HighQualityBicubic;
                    g.PixelOffsetMode = PixelOffsetMode.Half;
                    g.DrawImage(strip, r, new Rectangle(0, st * cell, cell, cell), GraphicsUnit.Pixel);
                    g.PixelOffsetMode = PixelOffsetMode.Default;
                }
                else DrawTile(g, r, i, st);
            }
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            int b = BtnAt(e.Location);
            if (btnDown >= 0 && b != btnDown) b = -1;
            if (b != btnHot) { btnHot = b; Invalidate(); }
            int h = -1;
            if (b < 0) for (int i = 0; i < tabs.Length; i++) if (tabs[i].Contains(e.Location)) h = i;
            if (h != hot) { hot = h; Invalidate(); }
            int a = b < 0 ? ArrowAt(e.Location) : -1;
            if (a != arrowHot) { arrowHot = a; Invalidate(); }
            Cursor = GameCursor.Arrow;
        }

        protected override void OnMouseLeave(EventArgs e) { hot = -1; arrowHot = -1; if (btnDown < 0) btnHot = -1; Invalidate(); }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            if (e.Button != MouseButtons.Left) return;
            int b = BtnAt(e.Location);
            if (b >= 0) { btnDown = b; btnHot = b; Capture = true; Invalidate(); return; }
            int arrow = ArrowAt(e.Location);
            if (arrow >= 0) { CyclePic(arrow == 0 ? -1 : 1); return; }
            for (int i = 0; i < tabs.Length; i++)
            {
                if (!tabs[i].Contains(e.Location)) continue;
                if (i < 3) { if ((int)Sort != i) { Sort = (SortBy)i; Invalidate(); if (SortChanged != null) SortChanged(this, EventArgs.Empty); } }
                else if ((int)Show != i - 3) { Show = (ShowFilter)(i - 3); Invalidate(); if (FilterChanged != null) FilterChanged(this, EventArgs.Empty); }
                return;
            }
            // anywhere else on the header moves the window
            var f = FindForm();
            if (f != null && f.WindowState == FormWindowState.Normal)
            {
                Win.ReleaseCapture();
                Win.SendMessage(f.Handle, Win.WM_NCLBUTTONDOWN, (IntPtr)Win.HTCAPTION, IntPtr.Zero);
            }
        }

        protected override void OnMouseUp(MouseEventArgs e)
        {
            int down = btnDown;
            btnDown = -1; Capture = false;
            if (down >= 0 && BtnAt(e.Location) == down)
            {
                var f = FindForm();
                if (f != null) { if (down == 0) f.WindowState = FormWindowState.Minimized; else f.Close(); }
            }
            btnHot = BtnAt(e.Location);
            Invalidate();
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            if (Width <= 0 || Height <= 0) return;
            var g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;
            using (var b = new LinearGradientBrush(ClientRectangle, Theme.BgTop, Color.FromArgb(9, 18, 30), LinearGradientMode.Vertical))
                g.FillRectangle(b, ClientRectangle);
            DrawBackground(g);

            int m = Theme.S(22);
            var pr = PicRect();
            using (var path = Theme.Round(pr, Theme.S(8)))
            {
                if (Pics().Count > 0)
                {
                    var clip = g.Clip; g.SetClip(path, CombineMode.Intersect);
                    g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                    g.PixelOffsetMode = PixelOffsetMode.Half;
                    g.DrawImage(Pics()[picIndex], pr);
                    g.PixelOffsetMode = PixelOffsetMode.Default;
                    g.Clip = clip;
                    DrawArrows(g);
                }
                else if (pic != null)
                {
                    var clip = g.Clip; g.SetClip(path, CombineMode.Intersect);
                    g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                    g.DrawImage(pic, pr);
                    g.Clip = clip;
                }
                else
                {
                    // No gamer picture: the tile (assets\tile.png, built in), else the program's icon.
                    if (tile == null) tile = Res.Image("tile.png");
                    if (tile == null) tile = Res.AppIcon().ToBitmap();
                    if (tile != null)
                    {
                        var clip = g.Clip; g.SetClip(path, CombineMode.Intersect);
                        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                        g.DrawImage(tile, pr);
                        g.Clip = clip;
                    }
                    else Theme.Medal(g, pr, true);
                }
                using (var pen = new Pen(Theme.Frame, Theme.S(2))) g.DrawPath(pen, path);
            }

            int tx = pr.Right + Theme.S(20);
            using (var b = new SolidBrush(Theme.Text)) g.DrawString(tag, tagFont, b, tx, m - Theme.S(6));
            string line = "Halo: Combat Evolved    " + count + " of " + total + " achievements    " + score + " of " + max;
            float ly = m + tagFont.Height;
            using (var b = new SolidBrush(Theme.TextDim)) g.DrawString(line, infoFont, b, tx, ly, Theme.Typo);
            float lw = Theme.TextW(g, line, infoFont);
            float gh = Theme.GSize(infoFont);
            Theme.GMark(g, new RectangleF(tx + lw + Theme.GGap, Theme.GTop(infoFont, ly), gh, gh), Color.White);

            // progress
            var bar = new RectangleF(tx, ly + infoFont.Height + Theme.S(10), Math.Max(10, Width - tx - m), Theme.S(8));
            using (var p = Theme.Round(bar, bar.Height / 2)) using (var b = new SolidBrush(Color.FromArgb(18, 36, 56))) g.FillPath(b, p);
            if (max > 0 && score > 0)
            {
                var fill = new RectangleF(bar.X, bar.Y, Math.Max(bar.Height, bar.Width * score / max), bar.Height);
                using (var p = Theme.Round(fill, bar.Height / 2)) using (var b = new LinearGradientBrush(fill, Theme.Accent, Color.FromArgb(40, 120, 190), LinearGradientMode.Horizontal)) g.FillPath(b, p);
            }

            // sort tabs
            int x = tx, y = Height - Theme.S(34);
            for (int i = 0; i < 3; i++)
            {
                var sz = g.MeasureString(TabNames[i], tabFont);
                tabs[i] = new Rectangle(x - Theme.S(6), y - Theme.S(4), (int)sz.Width + Theme.S(12), (int)sz.Height + Theme.S(8));
                bool on = (int)Sort == i;
                using (var b = new SolidBrush(on ? Theme.Accent : (hot == i ? Theme.Text : Theme.TextDim))) g.DrawString(TabNames[i], tabFont, b, x, y);
                if (on) using (var b = new SolidBrush(Theme.Accent)) g.FillRectangle(b, x, y + sz.Height + Theme.S(1), sz.Width - Theme.S(4), Theme.S(2));
                x += (int)sz.Width + Theme.S(22);
            }

            // show all / unlocked / locked, right-aligned on the same row
            float fw = 0;
            for (int i = 3; i < 6; i++) fw += g.MeasureString(TabNames[i], tabFont).Width + (i < 5 ? Theme.S(22) : 0);
            int fx = Math.Max(x + Theme.S(10), Width - Theme.S(22) - (int)fw);
            for (int i = 3; i < 6; i++)
            {
                var sz = g.MeasureString(TabNames[i], tabFont);
                tabs[i] = new Rectangle(fx - Theme.S(6), y - Theme.S(4), (int)sz.Width + Theme.S(12), (int)sz.Height + Theme.S(8));
                bool on = (int)Show == i - 3;
                using (var b = new SolidBrush(on ? Theme.Accent : (hot == i ? Theme.Text : Theme.TextDim))) g.DrawString(TabNames[i], tabFont, b, fx, y);
                if (on) using (var b = new SolidBrush(Theme.Accent)) g.FillRectangle(b, fx, y + sz.Height + Theme.S(1), sz.Width - Theme.S(4), Theme.S(2));
                fx += (int)sz.Width + Theme.S(22);
            }
            using (var pen = new Pen(Theme.Frame, 1)) g.DrawLine(pen, 0, Height - 1, Width, Height - 1);
            DrawButtons(g);
        }

        // The header's picture (assets\header.png): as wide as the header, the band of it at BackgroundAnchor
        // of its height shown, a little darkened so the text stays clear.
        static Image background;
        static bool backgroundLoaded;
        const float BackgroundAnchor = 0.72f;

        void DrawBackground(Graphics g)
        {
            if (!backgroundLoaded) { background = Res.Image("header.png"); backgroundLoaded = true; }
            if (background == null) return;
            float scale = Math.Max((float)Width / background.Width, (float)Height / background.Height);
            float w = background.Width * scale, h = background.Height * scale;
            float y = Math.Max(Height - h, Math.Min(0, Height / 2f - h * BackgroundAnchor));
            g.InterpolationMode = InterpolationMode.HighQualityBicubic;
            g.DrawImage(background, (Width - w) / 2, y, w, h);
            using (var b = new SolidBrush(Color.FromArgb(64, 9, 18, 30))) g.FillRectangle(b, ClientRectangle);
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing) { tagFont.Dispose(); infoFont.Dispose(); tabFont.Dispose(); if (pic != null) pic.Dispose(); }
            base.Dispose(disposing);
        }
    }

    // Scrolling list drawn by hand so it stays dark (mouse wheel, scrollbar drag, keys).
    class AchList : Control
    {
        Achievement[] all = new Achievement[0];
        List<Achievement> rows = new List<Achievement>();
        SortBy sort = SortBy.Game;
        int scroll, hot = -1, sel = -1;
        int tickRow;   // top row at the last scroll sound
        bool dragging; int dragStart, dragScroll;
        readonly Font nameFont = Theme.UiFont(17f, FontStyle.Bold), descFont = Theme.UiFont(14f, FontStyle.Regular), smallFont = Theme.UiFont(13f, FontStyle.Regular), scoreFont = Theme.UiFont(18f, FontStyle.Bold);
        int RowH { get { return Theme.S(88); } }
        int Gap { get { return Theme.S(8); } }
        int Edge { get { return Theme.S(18); } }

        protected override void WndProc(ref Message m)
        {
            if (m.Msg == Win.WM_NCHITTEST && Win.NearEdge(this, m.LParam)) { m.Result = (IntPtr)Win.HTTRANSPARENT; return; }
            base.WndProc(ref m);
        }

        public AchList()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.UserPaint | ControlStyles.ResizeRedraw | ControlStyles.Selectable, true);
            TabStop = true;
        }

        public SortBy SortMode { set { sort = value; Build(); scroll = 0; tickRow = 0; Invalidate(); } }

        ShowFilter filter = ShowFilter.All;
        public ShowFilter Filter { set { filter = value; sel = -1; hot = -1; Build(); scroll = 0; tickRow = 0; Invalidate(); } }

        public void Set(Achievement[] a) { all = a; Build(); Invalidate(); }

        void Build()
        {
            rows = new List<Achievement>();
            foreach (var a in all)
                if (filter == ShowFilter.All || (filter == ShowFilter.Unlocked) == a.Unlocked) rows.Add(a);
            if (sort == SortBy.Game)
                rows.Sort(delegate(Achievement x, Achievement y) { return Order(x).CompareTo(Order(y)); });
            else if (sort == SortBy.Date)
                rows.Sort(delegate(Achievement x, Achievement y)
                {
                    if (x.Unlocked != y.Unlocked) return x.Unlocked ? -1 : 1;
                    int c = y.UnlockedAt.CompareTo(x.UnlockedAt);
                    return c != 0 ? c : Order(x).CompareTo(Order(y));
                });
            else
                rows.Sort(delegate(Achievement x, Achievement y)
                {
                    if (x.Unlocked != y.Unlocked) return x.Unlocked ? -1 : 1;
                    int c = y.Score.CompareTo(x.Score);
                    return c != 0 ? c : Order(x).CompareTo(Order(y));
                });
            Clamp();
        }

        // The order of the achievement list (Data.cs).
        static int Order(Achievement a) { return Array.IndexOf(Data.All, a); }

        int ContentH { get { return Edge * 2 + rows.Count * (RowH + Gap) - Gap; } }
        int MaxScroll { get { return Math.Max(0, ContentH - Height); } }
        void Clamp() { scroll = Math.Max(0, Math.Min(scroll, MaxScroll)); }

        // Scroll sound each time the list moves past a row (wheel, scrollbar, page click).
        void ScrollTick()
        {
            int r = scroll / (RowH + Gap);
            if (r != tickRow) { tickRow = r; Sfx.Scroll(); }
        }

        Rectangle Thumb()
        {
            if (MaxScroll <= 0) return Rectangle.Empty;
            int track = Height - Theme.S(16);
            int th = Math.Max(Theme.S(40), track * Height / ContentH);
            int ty = Theme.S(8) + (track - th) * scroll / MaxScroll;
            return new Rectangle(Width - Theme.S(12), ty, Theme.S(6), th);
        }

        protected override void OnResize(EventArgs e) { Clamp(); base.OnResize(e); }

        protected override void OnMouseWheel(MouseEventArgs e)
        {
            scroll -= e.Delta * (RowH + Gap) / 120; Clamp(); ScrollTick(); Invalidate();
        }

        protected override void OnMouseDown(MouseEventArgs e)
        {
            Focus();
            var t = Thumb();
            if (!t.IsEmpty && e.X >= Width - Theme.S(18))
            {
                if (t.Contains(new Point(t.X, e.Y))) { dragging = true; dragStart = e.Y; dragScroll = scroll; Capture = true; }
                else { scroll += (e.Y < t.Y ? -1 : 1) * Height; Clamp(); ScrollTick(); Invalidate(); }
                return;
            }
            sel = RowAt(e.Y); Invalidate();
        }

        protected override void OnMouseMove(MouseEventArgs e)
        {
            if (dragging)
            {
                var t = Thumb();
                int track = Height - Theme.S(16) - t.Height;
                if (track > 0) { scroll = dragScroll + (e.Y - dragStart) * MaxScroll / track; Clamp(); ScrollTick(); Invalidate(); }
                return;
            }
            int h = RowAt(e.Y);
            if (h != hot) { hot = h; Invalidate(); }
        }

        protected override void OnMouseUp(MouseEventArgs e) { dragging = false; Capture = false; }
        protected override void OnMouseLeave(EventArgs e) { hot = -1; Invalidate(); }

        protected override bool IsInputKey(Keys k)
        {
            return k == Keys.Up || k == Keys.Down || k == Keys.Left || k == Keys.Right || k == Keys.PageUp || k == Keys.PageDown ||
                k == Keys.Home || k == Keys.End || base.IsInputKey(k);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            int n = rows.Count;
            if (n == 0) return;
            if (e.KeyCode == Keys.Down) MoveSelection(1, false);
            else if (e.KeyCode == Keys.Up) MoveSelection(-1, false);
            else if (e.KeyCode == Keys.PageDown) MoveSelection(1, true);
            else if (e.KeyCode == Keys.PageUp) MoveSelection(-1, true);
            else if (e.KeyCode == Keys.Home) SelectRow(0);
            else if (e.KeyCode == Keys.End) SelectRow(n - 1);
        }

        // a row, or a page of them, up (-1) or down (1), brought into view; the scroll sound when it moved
        public void MoveSelection(int step, bool page)
        {
            int per = Math.Max(1, Height / (RowH + Gap));
            if (rows.Count == 0) return;
            if (sel < 0) { SelectRow(step > 0 ? 0 : rows.Count - 1); return; }
            SelectRow(sel + step * (page ? per : 1));
        }

        void SelectRow(int row)
        {
            int n = rows.Count;
            if (n == 0) return;
            row = Math.Max(0, Math.Min(n - 1, row));
            if (row != sel) Sfx.Scroll();
            sel = row;
            int top = Edge + sel * (RowH + Gap);
            if (top < scroll) scroll = top - Edge;
            if (top + RowH > scroll + Height) scroll = top + RowH - Height + Edge;
            Clamp();
            tickRow = scroll / (RowH + Gap);
            Invalidate();
        }

        int RowAt(int y)
        {
            int cy = y + scroll - Edge;
            if (cy < 0) return -1;
            int i = cy / (RowH + Gap);
            if (i >= rows.Count || cy % (RowH + Gap) >= RowH) return -1;
            return i;
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            if (Width <= 0 || Height <= 0) return;
            var g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;
            using (var b = new LinearGradientBrush(ClientRectangle, Color.FromArgb(9, 18, 30), Theme.BgBottom, LinearGradientMode.Vertical))
                g.FillRectangle(b, ClientRectangle);

            int w = Width - Edge * 2 - Theme.S(10);
            if (rows.Count == 0)
            {
                string msg = filter == ShowFilter.Unlocked ? "No achievements unlocked yet." : "All achievements unlocked.";
                using (var b = new SolidBrush(Theme.TextDim))
                using (var sf = new StringFormat { Alignment = StringAlignment.Center })
                    g.DrawString(msg, nameFont, b, new RectangleF(0, Edge * 2, Width, nameFont.Height * 2), sf);
            }
            int first = Math.Max(0, (scroll - Edge) / (RowH + Gap));
            for (int i = first; i < rows.Count; i++)
            {
                int y = Edge + i * (RowH + Gap) - scroll;
                if (y > Height) break;
                DrawRow(g, rows[i], new Rectangle(Edge, y, w, RowH), i == hot, i == sel);
            }

            var t = Thumb();
            if (!t.IsEmpty)
                using (var p = Theme.Round(t, t.Width / 2f)) using (var b = new SolidBrush(dragging ? Theme.Accent : Theme.Frame)) g.FillPath(b, p);
        }

        void DrawRow(Graphics g, Achievement a, Rectangle r, bool isHot, bool isSel)
        {
            using (var path = Theme.Round(r, Theme.S(6)))
            {
                Color top = a.Unlocked ? Theme.RowTop : Color.FromArgb(16, 26, 38);
                Color bot = a.Unlocked ? Theme.RowBottom : Color.FromArgb(9, 15, 23);
                using (var b = new LinearGradientBrush(r, top, bot, LinearGradientMode.Vertical)) g.FillPath(b, path);
                Color border = isSel ? Theme.Accent : isHot ? Color.FromArgb(70, 130, 180) : Color.FromArgb(a.Unlocked ? 255 : 120, Theme.Frame);
                using (var pen = new Pen(border, isSel ? 1.6f : 1f)) g.DrawPath(pen, path);
            }

            int pad = Theme.S(12), isz = r.Height - pad * 2;
            Icons.Draw(g, a, new Rectangle(r.X + pad, r.Y + pad, isz, isz));

            // gamerscore at the right
            string sc = a.Score.ToString();
            float sw = Theme.TextW(g, sc, scoreFont);
            float gh = Theme.GSize(scoreFont);
            float sx = r.Right - pad - gh - Theme.GGap - sw;
            Color sCol = a.Unlocked ? Theme.Text : Theme.TextDim;
            using (var b = new SolidBrush(sCol)) g.DrawString(sc, scoreFont, b, sx, r.Y + pad - Theme.S(2), Theme.Typo);
            Theme.GMark(g, new RectangleF(sx + sw + Theme.GGap, Theme.GTop(scoreFont, r.Y + pad - Theme.S(2)), gh, gh), a.Unlocked ? Color.White : Theme.TextDim);

            float tx = r.X + pad + isz + Theme.S(14);
            float tw = sx - tx - Theme.S(10);
            using (var b = new SolidBrush(a.Unlocked ? Theme.Text : Color.FromArgb(170, 190, 210)))
                g.DrawString(a.Name, nameFont, b, new RectangleF(tx, r.Y + pad - Theme.S(3), tw, nameFont.Height + 2), new StringFormat(StringFormatFlags.NoWrap) { Trimming = StringTrimming.EllipsisCharacter });
            using (var b = new SolidBrush(Theme.TextDim))
                g.DrawString(a.Desc, descFont, b, new RectangleF(tx, r.Y + pad + nameFont.Height - Theme.S(2), r.Right - pad - tx, descFont.Height * 2 + 2));

            string when = a.Unlocked ? "Unlocked " + a.When.ToString("d MMMM yyyy, HH:mm") : "Locked";
            var wsz = g.MeasureString(when, smallFont);
            using (var b = new SolidBrush(a.Unlocked ? Theme.Gold : Color.FromArgb(110, 130, 150)))
                g.DrawString(when, smallFont, b, r.Right - pad - wsz.Width, r.Bottom - pad - smallFont.Height + Theme.S(2));
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing) { nameFont.Dispose(); descFont.Dispose(); smallFont.Dispose(); scoreFont.Dispose(); }
            base.Dispose(disposing);
        }
    }
}
