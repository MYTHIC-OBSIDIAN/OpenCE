using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace OpenCEAchievements
{
	// A cursor of your own, embedded from assets\cursor.cur (Build.bat); else the Windows arrow.
	// Loaded through Windows (LoadCursorFromFile) so it keeps its colours, transparency and size.
	internal static class GameCursor
	{
		[DllImport("user32.dll", EntryPoint = "LoadCursorFromFileW", CharSet = CharSet.Unicode)]
		private static extern IntPtr LoadCursorFromFile(string fileName);

		private static Cursor arrow;

		public static Cursor Arrow
		{
			get
			{
				if (arrow == null)
				{
					try
					{
						using (Stream src = Assembly.GetExecutingAssembly().GetManifestResourceStream("cursor.cur"))
						{
							if (src != null)
							{
								string path = Path.Combine(Path.GetTempPath(), "OpenCEAchievements_cursor.cur");
								if (!File.Exists(path) || new FileInfo(path).Length != src.Length)
								{
									using (FileStream dst = File.Create(path))
									{
										src.CopyTo(dst);
									}
								}
								IntPtr handle = LoadCursorFromFile(path);
								if (handle != IntPtr.Zero)
								{
									arrow = new Cursor(handle);
								}
							}
						}
					}
					catch
					{
					}
					if (arrow == null)
					{
						arrow = Cursors.Default;
					}
				}
				return arrow;
			}
		}
	}
}
