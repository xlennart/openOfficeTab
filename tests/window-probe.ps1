if(-not ('OfficeLayoutProbe' -as [type])){
Add-Type -TypeDefinition @'
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public static class OfficeLayoutProbe {
 public struct Rect { public int Left,Top,Right,Bottom; }
 [StructLayout(LayoutKind.Sequential)] public struct Monitor {public int Size;public Rect Full,Work;public int Flags;}
 public delegate bool Callback(IntPtr window,IntPtr data);
 [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr value);
 [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr window,out Rect rect);
 [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int width,int height,uint flags);
 [DllImport("user32.dll")] static extern IntPtr MonitorFromWindow(IntPtr window,int flags);
 [DllImport("user32.dll")] static extern bool GetMonitorInfo(IntPtr monitor,ref Monitor info);
 [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr window,Callback callback,IntPtr data);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window,StringBuilder name,int length);
 [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr window);
 public static Rect Bounds(long window){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{Rect r;if(!GetWindowRect(new IntPtr(window),out r))throw new Exception("Window bounds unavailable");return r;}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
 public static Rect WorkArea(long window){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{var m=new Monitor{Size=Marshal.SizeOf(typeof(Monitor))};if(!GetMonitorInfo(MonitorFromWindow(new IntPtr(window),2),ref m))throw new Exception("Monitor unavailable");return m.Work;}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
 public static void Place(long window,Rect r){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{if(!SetWindowPos(new IntPtr(window),IntPtr.Zero,r.Left,r.Top,r.Right-r.Left,r.Bottom-r.Top,0x14))throw new Exception("Placement failed");}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
 public static bool Same(Rect a,Rect b){return a.Left==b.Left&&a.Top==b.Top&&a.Right==b.Right&&a.Bottom==b.Bottom;}
 public static bool RibbonVisible(long window){var root=new IntPtr(window);bool found=false;var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{EnumChildWindows(root,(w,d)=>{var name=new StringBuilder(128);GetClassName(w,name,128);if(name.ToString()!="MsoCommandBar"||!IsWindowVisible(w))return true;Rect ribbon;GetWindowRect(w,out ribbon);if(ribbon.Bottom-ribbon.Top<=96*GetDpiForWindow(w)/96)return true;EnumChildWindows(w,(child,unused)=>{var cls=new StringBuilder(128);GetClassName(child,cls,128);if(cls.ToString()!="NetUIHWND"||!IsWindowVisible(child))return true;Rect content;GetWindowRect(child,out content);if(Math.Min(content.Right,ribbon.Right)-Math.Max(content.Left,ribbon.Left)>=(ribbon.Right-ribbon.Left)*3/4&&Math.Min(content.Bottom,ribbon.Bottom)-Math.Max(content.Top,ribbon.Top)>=(ribbon.Bottom-ribbon.Top)*3/4)found=true;return !found;},IntPtr.Zero);return !found;},IntPtr.Zero);return found;}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
}
'@
}
