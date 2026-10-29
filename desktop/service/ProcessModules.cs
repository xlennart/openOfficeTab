using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

namespace OpenOfficeTab.Desktop {
 internal static class ProcessModules {
  [DllImport("kernel32.dll",SetLastError=true)]static extern IntPtr OpenProcess(uint access,bool inherit,int pid);
  [DllImport("kernel32.dll",SetLastError=true)]static extern bool K32EnumProcessModulesEx(IntPtr process,[Out]IntPtr[] modules,uint bytes,out uint needed,uint filter);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]static extern uint K32GetModuleFileNameEx(IntPtr process,IntPtr module,StringBuilder name,int capacity);
  [DllImport("kernel32.dll")]static extern bool CloseHandle(IntPtr handle);
  internal static string[] Paths(int pid){
   IntPtr process=OpenProcess(0x410,false,pid);
   if(process==IntPtr.Zero)throw new Win32Exception(Marshal.GetLastWin32Error());
   try{
    var modules=new IntPtr[512];
    for(int attempt=0;attempt<4;attempt++){
     uint needed,bytes=checked((uint)(modules.Length*IntPtr.Size));
     // The service is x64; explicitly include WOW64 modules as well.
     if(!K32EnumProcessModulesEx(process,modules,bytes,out needed,3))throw new Win32Exception(Marshal.GetLastWin32Error());
     if(needed>bytes){if(needed>(uint)(8192*IntPtr.Size))throw new InvalidOperationException("Module list exceeded its bound.");modules=new IntPtr[(needed+(uint)IntPtr.Size-1)/(uint)IntPtr.Size];continue;}
     if(needed==0||needed%(uint)IntPtr.Size!=0)throw new InvalidOperationException("Module list is unavailable.");
     var result=new List<string>();
     for(int index=0;index<needed/(uint)IntPtr.Size;index++){
      var name=new StringBuilder(32768);
      uint length=K32GetModuleFileNameEx(process,modules[index],name,name.Capacity);
      if(length==0)throw new Win32Exception(Marshal.GetLastWin32Error());
      if(length>=name.Capacity-1)throw new InvalidOperationException("Module path exceeded its bound.");
      result.Add(name.ToString());
     }
     return result.ToArray();
    }
    throw new InvalidOperationException("Module list changed during inspection.");
   }finally{CloseHandle(process);}
  }
 }
}
