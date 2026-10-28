using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;
using Microsoft.Win32;

namespace OpenOfficeTab.Desktop {
 internal static class DesktopProgram {internal const string Repo="https://github.com/xlennart/openOfficetab",Author="https://github.com/xlennart";}
 internal sealed class Services {
  internal static readonly Version Version=new Version(0,2,0);
  internal readonly StateStore Store;internal bool Preview=false;
  internal Services(string root){Store=new StateStore(root);}
  internal static bool OfficeRunning(){foreach(string name in new[]{"WINWORD","EXCEL","POWERPNT"}){var processes=Process.GetProcessesByName(name);bool found=processes.Length>0;foreach(var process in processes)process.Dispose();if(found)return true;}return false;}
 }
 // No WinForms dependency: the Tauri shell owns all visible UI.
 public static class Api {
  static readonly JavaScriptSerializer Json=new JavaScriptSerializer{MaxJsonLength=4*1024*1024};
  public static string DiagnosticTimeline(string root){return Json.Serialize(DiagnosticLog.Snapshot(root));}
  public static void RecordAction(string root,string action,string phase,int hr,long elapsed){DiagnosticLog.Action(root,action,phase,hr,elapsed);}
  public static string Read(string root){return Json.Serialize(new StateStore(root).Read());}
  public static void Merge(string root,string section,string changes){new StateStore(root).Merge(section,Json.Deserialize<Dictionary<string,string>>(changes));}
  public static string Hash(string file){return StateStore.Sha256(file);}
  public static string Identity(string root){return StateStore.Identity(root);}
  public static bool OfficeRunning(){return Services.OfficeRunning();}
  public static string OfficeArchitecture(){
   foreach(var view in new[]{RegistryView.Registry64,RegistryView.Registry32})using(var machine=RegistryKey.OpenBaseKey(RegistryHive.LocalMachine,view)){
    using(var config=machine.OpenSubKey(@"SOFTWARE\Microsoft\Office\ClickToRun\Configuration")){if(config!=null){string platform=config.GetValue("Platform","") as string;if(platform=="x64"||platform=="x86")return platform;}}
    foreach(string name in new[]{"WINWORD.EXE","EXCEL.EXE","POWERPNT.EXE"})using(var entry=machine.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\"+name)){
     string path=entry==null?null:entry.GetValue("") as string;if(string.IsNullOrEmpty(path)||!File.Exists(path))continue;
     using(var stream=File.OpenRead(path))using(var reader=new BinaryReader(stream)){if(reader.ReadUInt16()!=0x5a4d)continue;stream.Position=0x3c;int offset=reader.ReadInt32();if(offset<0||offset>stream.Length-6)continue;stream.Position=offset;if(reader.ReadUInt32()!=0x4550)continue;ushort architecture=reader.ReadUInt16();if(architecture==0x8664)return "x64";if(architecture==0x14c)return "x86";}
    }
   }return "unknown";
  }
  public static string InspectHosts(){
   var result=new List<object>();string registered=(string)Registry.GetValue(@"HKEY_CURRENT_USER\Software\Classes\CLSID\{65F17021-3B6B-4C5E-B54B-512806431710}\InprocServer32","","");
   string[] hosts={"Word","Excel","PowerPoint"},names={"WINWORD","EXCEL","POWERPNT"};
   for(int index=0;index<hosts.Length;index++){
    bool hasKey=false,enabled=false,loaded=false,readable=true,matches=true;int running=0;var builds=new HashSet<string>();
    using(var key=Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Office\"+hosts[index]+@"\Addins\openOfficetab.Connect")){hasKey=key!=null;enabled=hasKey&&Convert.ToInt32(key.GetValue("LoadBehavior",0))==3;}
    foreach(var process in Process.GetProcessesByName(names[index]))using(process){running++;try{string build=process.MainModule.FileVersionInfo.FileVersion;if(System.Text.RegularExpressions.Regex.IsMatch(build??"",@"\A[0-9.]{3,40}\z"))builds.Add(build);foreach(ProcessModule module in process.Modules)if(string.Equals(module.ModuleName,"openOfficetab.dll",StringComparison.OrdinalIgnoreCase)){loaded=true;matches&=string.Equals(Path.GetFullPath(module.FileName),Path.GetFullPath(registered),StringComparison.OrdinalIgnoreCase);}}catch(System.ComponentModel.Win32Exception){readable=false;}catch(InvalidOperationException){readable=false;}catch(ArgumentException){readable=false;}}
    result.Add(new {name=hosts[index],registered=hasKey,enabled=enabled,dllExists=File.Exists(registered),running=running,officeBuilds=builds.OrderBy(b=>b).ToArray(),loaded=loaded,moduleReadable=readable,matchesRegistration=loaded&&matches});
   }return Json.Serialize(result);
  }
  public static string CheckUpdate(string root,bool force){var service=new Services(root);var client=new UpdateClient(service);var release=client.Check(force);return Json.Serialize(release==null?new {available=false,version="",size=0L}:new {available=client.IsNewer(release),version=release.Version.ToString(),size=release.Size});}
  public static string DownloadUpdate(string root,string version){var service=new Services(root);var client=new UpdateClient(service);var release=client.Check(false);if(release==null||release.Version.ToString()!=version||!client.IsNewer(release))throw new InvalidDataException("Update metadata changed; check again.");return client.Download(release);}
  public static void InstallUpdate(string root,string version){var service=new Services(root);var client=new UpdateClient(service);var release=client.Check(false);if(release==null||release.Version.ToString()!=version||!client.IsNewer(release))throw new InvalidDataException("Update metadata changed; check again.");client.Launch(release,client.Download(release),true);}
  public static string ParseRelease(string metadata){var release=UpdateClient.Parse(metadata);return Json.Serialize(release==null?null:new {version=release.Version.ToString(),name=release.Name,hash=release.Hash,size=release.Size});}
  public static bool AllowedDownload(string uri){return UpdateClient.AllowedDownload(new Uri(uri));}
  public static string ProbeRoutes(string root){return Json.Serialize(new UpdateRoutes(root).Probe());}
 }
}
