using System;
using System.IO;
using System.Runtime.InteropServices;
internal static class NativePayloadTest {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern IntPtr LoadLibraryW(string name);
 [DllImport("kernel32.dll",CharSet=CharSet.Ansi,ExactSpelling=true)]static extern IntPtr GetProcAddress(IntPtr module,string name);
 [DllImport("kernel32.dll")]static extern bool FreeLibrary(IntPtr module);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)]delegate int CanUnload();
 [UnmanagedFunctionPointer(CallingConvention.StdCall)]delegate int GetClass(ref Guid clsid,ref Guid iid,out IntPtr value);
 static int Main(string[] args){try{
  if(IntPtr.Size!=int.Parse(args[1]))throw new Exception("Test process architecture mismatch.");
  var module=LoadLibraryW(Path.GetFullPath(args[0]));if(module==IntPtr.Zero)throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
  try{
   foreach(string name in new[]{"DllGetClassObject","DllCanUnloadNow","DllRegisterServer","DllUnregisterServer","DllInstall"})if(GetProcAddress(module,name)==IntPtr.Zero)throw new Exception("Missing undecorated export: "+name);
   var unload=(CanUnload)Marshal.GetDelegateForFunctionPointer(GetProcAddress(module,"DllCanUnloadNow"),typeof(CanUnload));
   if(unload()!=0)throw new Exception("Unexpected initial DLL unload state.");
   var getClass=(GetClass)Marshal.GetDelegateForFunctionPointer(GetProcAddress(module,"DllGetClassObject"),typeof(GetClass));
   var unknown=new Guid("81897D74-B48B-4DB1-9E77-AB9F21A82243");var iid=new Guid("00000001-0000-0000-C000-000000000046");IntPtr value;
   if(getClass(ref unknown,ref iid,out value)!=unchecked((int)0x80040111)||value!=IntPtr.Zero)throw new Exception("COM export calling convention/result mismatch.");
   var known=new Guid("65F17021-3B6B-4C5E-B54B-512806431710");
   if(getClass(ref known,ref iid,out value)!=0||value==IntPtr.Zero)throw new Exception("COM class factory export failed.");
   Marshal.Release(value);if(unload()!=0)throw new Exception("COM factory did not release its DLL lifetime.");
   Console.WriteLine("Native payload load, five exports and COM ABI passed: "+(IntPtr.Size*8)+"-bit. No registration or Office startup.");
  }finally{FreeLibrary(module);}return 0;
 }catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
