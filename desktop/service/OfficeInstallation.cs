using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Microsoft.Win32;

namespace OpenOfficeTab.Desktop {
 // Office's process architecture selects the in-process DLL. The center stays
 // x64; Windows architecture alone must never select an Office payload.
 internal static class OfficeInstallation {
  internal static string ExecutableArchitecture(string path) {
   try {
    using(var stream=File.OpenRead(path))using(var reader=new BinaryReader(stream)) {
     if(stream.Length<64||reader.ReadUInt16()!=0x5a4d)return "unknown";
     stream.Position=0x3c;int offset=reader.ReadInt32();
     if(offset<64||offset>1024*1024||offset>stream.Length-24)return "unknown";
     stream.Position=offset;if(reader.ReadUInt32()!=0x4550)return "unknown";
     ushort machine=reader.ReadUInt16();
     return machine==0x14c?"x86":machine==0x8664?"x64":machine==0xaa64?"arm64":"unknown";
    }
   }catch(IOException){return "unknown";}catch(UnauthorizedAccessException){return "unknown";}catch(System.Security.SecurityException){return "unknown";}
  }
  internal static string Resolve(IEnumerable<string> executables,IEnumerable<string> declarations) {
   var actual=executables.ToArray();
   if(actual.Length==0||actual.Any(value=>value=="unknown"))return "unknown";
   var unique=actual.Distinct().ToArray();if(unique.Length!=1)return "conflict";
   if(unique[0]!="x86"&&unique[0]!="x64")return "unsupported";
   if(declarations.Any(value=>(value=="x86"||value=="x64")&&value!=unique[0]))return "conflict";
   return unique[0];
  }
  internal static string Select(string requested,string detected,bool windows64) {
   if(!windows64)throw new InvalidDataException("此安装器需要 64 位 Windows，支持 32 位或 64 位 Office。");
   if(detected=="conflict")throw new InvalidDataException("Office 位数信息存在冲突，请检查 Office 安装后重新检测。");
   if(detected!="x86"&&detected!="x64")throw new InvalidDataException("无法确认受支持的 Office 位数，未执行安装。");
   if(requested!="auto"&&requested!="x86"&&requested!="x64")throw new InvalidDataException("无效的插件架构。");
   if(requested!="auto"&&requested!=detected)throw new InvalidDataException("所选插件与 Office 位数不匹配，未执行安装。");
   return detected;
  }
  static string Value(RegistryKey parent,string path,string name) {
   using(var key=parent.OpenSubKey(path)){return key==null?null:key.GetValue(name) as string;}
  }
  internal static string Detect() {
   var files=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var declarations=new List<string>();
   foreach(var view in Environment.Is64BitOperatingSystem?new[]{RegistryView.Registry64,RegistryView.Registry32}:new[]{RegistryView.Registry32}) {
    using(var machine=RegistryKey.OpenBaseKey(RegistryHive.LocalMachine,view))using(var user=RegistryKey.OpenBaseKey(RegistryHive.CurrentUser,view)) {
     const string config=@"SOFTWARE\Microsoft\Office\ClickToRun\Configuration";
     string platform=Value(machine,config,"Platform");if(platform!=null)declarations.Add(platform.Trim().ToLowerInvariant());
     string installation=Value(machine,config,"InstallationPath");
     foreach(var host in new[]{new[]{"Word","WINWORD.EXE"},new[]{"Excel","EXCEL.EXE"},new[]{"PowerPoint","POWERPNT.EXE"}}) {
      string appPath=@"Software\Microsoft\Windows\CurrentVersion\App Paths\"+host[1];
      foreach(var hive in new[]{machine,user}) {
       string file=Value(hive,appPath,"");if(!string.IsNullOrWhiteSpace(file)&&File.Exists(file.Trim().Trim('"')))files.Add(file.Trim().Trim('"'));
       foreach(string version in new[]{"16.0","15.0","14.0"})foreach(string component in new[]{host[0],"Common"}) {
        string directory=Value(hive,@"Software\Microsoft\Office\"+version+"\\"+component+@"\InstallRoot","Path");
        if(!string.IsNullOrWhiteSpace(directory)){string candidate=Path.Combine(directory,host[1]);if(File.Exists(candidate))files.Add(candidate);}
       }
      }
      if(!string.IsNullOrWhiteSpace(installation))foreach(string suffix in new[]{@"root\Office16","Office16",""}) {
       string candidate=Path.Combine(installation,suffix,host[1]);if(File.Exists(candidate))files.Add(candidate);
      }
     }
    }
   }
   return Resolve(files.Select(ExecutableArchitecture),declarations);
  }
  internal static string RegisteredDll(string architecture) {
   if(architecture!="x86"&&architecture!="x64")return "";
   var view=architecture=="x86"?RegistryView.Registry32:RegistryView.Registry64;
   using(var user=RegistryKey.OpenBaseKey(RegistryHive.CurrentUser,view))
    return Value(user,@"Software\Classes\CLSID\{65F17021-3B6B-4C5E-B54B-512806431710}\InprocServer32","")??"";
  }
  internal static bool MatchesPayload(string root,string architecture) {
   if(architecture!="x86"&&architecture!="x64")return false;
   string registered=RegisteredDll(architecture),payload=Path.Combine(root,"dist",architecture,"openOfficetab.dll");
   return File.Exists(registered)&&File.Exists(payload)&&ExecutableArchitecture(registered)==architecture&&ExecutableArchitecture(payload)==architecture&&StateStore.Sha256(registered)==StateStore.Sha256(payload);
  }
 }
}
