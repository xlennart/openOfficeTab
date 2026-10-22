Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class ReferenceInput {
 [StructLayout(LayoutKind.Sequential)]struct Key {public ushort Vk,Scan;public uint Flags,Time;public UIntPtr Extra;}
 [StructLayout(LayoutKind.Sequential)]struct Mouse {public int X,Y;public uint Data,Flags,Time;public UIntPtr Extra;}
 [StructLayout(LayoutKind.Explicit)]struct Union {[FieldOffset(0)]public Key Key;[FieldOffset(0)]public Mouse Mouse;}
 [StructLayout(LayoutKind.Sequential)]struct Input {public uint Type;public Union Data;}
 public struct Point {public int X,Y;} public struct Rect {public int Left,Top,Right,Bottom;public override string ToString(){return Left+","+Top+","+Right+","+Bottom;}}
 public static uint ProcessId; public static int LastX,LastY; public static long LastHit; public static object Position(){return new{X=LastX,Y=LastY,Hit=LastHit,Root=GetAncestor(new IntPtr(LastHit),2).ToInt64(),Foreground=GetForegroundWindow().ToInt64()};}
 [DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")]static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]static extern bool GetWindowRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")]static extern IntPtr GetAncestor(IntPtr w,uint flag);
 [DllImport("user32.dll")]static extern IntPtr WindowFromPoint(Point p);
 [DllImport("user32.dll")]static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")]static extern void SwitchToThisWindow(IntPtr w,bool altTab);


 [DllImport("user32.dll")]static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")]static extern IntPtr SendMessageTimeout(IntPtr w,uint message,IntPtr wp,IntPtr lp,uint flags,uint timeout,out IntPtr result);
 [DllImport("user32.dll")]static extern bool RedrawWindow(IntPtr w,IntPtr rect,IntPtr region,uint flags);
 [DllImport("user32.dll")]static extern bool ClientToScreen(IntPtr w,ref Point p);
 [DllImport("user32.dll")]static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll")]static extern uint SendInput(uint n,Input[] input,int size);
 public static uint Process(long w){uint p;if(GetWindowThreadProcessId(new IntPtr(w),out p)==0)return 0;return p;}
 public static void Bind(long root){ProcessId=Process(root);if(ProcessId==0)throw new Exception("Test workbook root no longer exists");} public static void Guard(){if(Process(GetForegroundWindow().ToInt64())!=ProcessId)throw new Exception("Test Excel lost foreground; stopped input (expected "+ProcessId+", actual "+Process(GetForegroundWindow().ToInt64())+")");}
 public static void Raise(long w){if(Process(w)!=ProcessId)throw new Exception("Foreign or stale root");for(int n=0;n<12;n++){SetForegroundWindow(new IntPtr(w));SwitchToThisWindow(new IntPtr(w),false);System.Threading.Thread.Sleep(150);if(GetForegroundWindow().ToInt64()==w)return;}}
 public static void Text(char c){Guard();var k=new Key{Scan=c,Flags=4};var up=k;up.Flags|=2;Send(new[]{new Input{Type=1,Data=new Union{Key=k}},new Input{Type=1,Data=new Union{Key=up}}});}
 public static void Press(ushort vk){Guard();var k=new Key{Vk=vk};var up=k;up.Flags=2;Send(new[]{new Input{Type=1,Data=new Union{Key=k}},new Input{Type=1,Data=new Union{Key=up}}});}
 static void Send(Input[] inputs){if(SendInput((uint)inputs.Length,inputs,Marshal.SizeOf(typeof(Input)))!=inputs.Length)throw new Exception("Input dispatch rejected");}
 public static void Screen(int x,int y){Guard();var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{if(Process(WindowFromPoint(new Point{X=x,Y=y}).ToInt64())!=ProcessId)throw new Exception("Target point is outside test Excel");LastX=x;LastY=y;LastHit=WindowFromPoint(new Point{X=x,Y=y}).ToInt64();SetCursorPos(x,y);Send(new[]{new Input{Data=new Union{Mouse=new Mouse{Flags=2}}},new Input{Data=new Union{Mouse=new Mouse{Flags=4}}}});}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
 public static void DirectTab(long w,int x,int y){if(Process(w)!=ProcessId)throw new Exception("Foreign or stale bar");var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{RedrawWindow(new IntPtr(w),IntPtr.Zero,IntPtr.Zero,0x101);IntPtr result;var point=new IntPtr((y<<16)|(x&65535));if(SendMessageTimeout(new IntPtr(w),0x201,new IntPtr(1),point,3,1500,out result)==IntPtr.Zero||SendMessageTimeout(new IntPtr(w),0x202,IntPtr.Zero,point,3,1500,out result)==IntPtr.Zero)throw new Exception("Tab handler did not respond");}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
 public static void Client(long w,int x,int y){if(Process(w)!=ProcessId)throw new Exception("Foreign or stale bar");var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{RedrawWindow(new IntPtr(w),IntPtr.Zero,IntPtr.Zero,0x101);var p=new Point{X=x,Y=y};ClientToScreen(new IntPtr(w),ref p);var hit=WindowFromPoint(p);Rect expectedRect,hitRect;GetWindowRect(new IntPtr(w),out expectedRect);GetWindowRect(hit,out hitRect);if(hit.ToInt64()!=w&&(Process(hit.ToInt64())!=ProcessId||expectedRect.Left!=hitRect.Left||expectedRect.Top!=hitRect.Top||expectedRect.Right!=hitRect.Right||expectedRect.Bottom!=hitRect.Bottom))throw new Exception("Expected tab bar "+w+" rect "+expectedRect+", hit rect "+hitRect+" root "+GetAncestor(new IntPtr(w),2).ToInt64()+", hit "+hit.ToInt64()+" root "+GetAncestor(hit,2).ToInt64()+", foreground "+GetForegroundWindow().ToInt64());Screen(p.X,p.Y);}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}
}
'@
