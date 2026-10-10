if(-not ('OfficeRot' -as [type])){
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
public static class OfficeRot {
 [DllImport("ole32.dll",CharSet=CharSet.Unicode)] static extern int CLSIDFromProgID(string s,out Guid g);
 [DllImport("oleaut32.dll")] static extern int GetActiveObject(ref Guid g,IntPtr reserved,[MarshalAs(UnmanagedType.IUnknown)] out object o);
 [DllImport("ole32.dll")] static extern int GetRunningObjectTable(int reserved,out IRunningObjectTable table);
 [DllImport("ole32.dll")] static extern int CreateBindCtx(int reserved,out IBindCtx context);
 public static object Attach(string name){Guid g;Marshal.ThrowExceptionForHR(CLSIDFromProgID(name+".Application",out g));object o;Marshal.ThrowExceptionForHR(GetActiveObject(ref g,IntPtr.Zero,out o));return o;}
 // BindToMoniker can launch another automation instance when a file is not
 // registered yet. Enumerate only already-running objects instead.
 public static object AttachFile(string path){
  IRunningObjectTable table=null;IBindCtx context=null;IEnumMoniker items=null;
  try{
   Marshal.ThrowExceptionForHR(GetRunningObjectTable(0,out table));
   Marshal.ThrowExceptionForHR(CreateBindCtx(0,out context));
   table.EnumRunning(out items);IMoniker[] current=new IMoniker[1];
   while(items.Next(1,current,IntPtr.Zero)==0){
    try{string name;current[0].GetDisplayName(context,null,out name);
     if(name.IndexOf(path,StringComparison.OrdinalIgnoreCase)>=0){object value;table.GetObject(current[0],out value);return value;}
    }catch(COMException){}finally{Marshal.ReleaseComObject(current[0]);}
   }
   throw new COMException("Fixture is not registered in the running object table yet.",unchecked((int)0x800401E3));
  }finally{if(items!=null)Marshal.ReleaseComObject(items);if(context!=null)Marshal.ReleaseComObject(context);if(table!=null)Marshal.ReleaseComObject(table);}
 }
}
'@
}
