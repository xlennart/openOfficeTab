using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;
using Microsoft.Win32;

namespace OpenOfficeTab.Desktop {
 // Keep signed asset URL spelling compatible with existing clients after the display-name rename.
 internal static class DesktopProgram {internal const string Repo="https://github.com/xlennart/openOfficetab",Author="https://github.com/xlennart";}
 internal sealed class Services {
  internal const string DisplayVersion="0.2.3-beta.1";
  internal static readonly Version Version=ReleaseVersion.Parse(DisplayVersion).Core;
  internal bool AcceptsPrerelease {get{var section=Store.Section("desktop");return section.ContainsKey("updateChannel")&&section["updateChannel"]=="beta";}}
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
  public static string Language(string root){return Json.Serialize(DesktopLanguage.Read(root));}
  public static string ResolveLanguage(int mode,string systemLocale){return DesktopLanguage.Resolve(mode,systemLocale);}
  public static string LocalizedError(string message,string locale,int hr){return DesktopLanguage.Error(message,locale,hr);}
  public static void Merge(string root,string section,string changes){new StateStore(root).Merge(section,Json.Deserialize<Dictionary<string,string>>(changes));}
  public static string Hash(string file){return StateStore.Sha256(file);}
  public static string Identity(string root){return StateStore.Identity(root);}
  public static bool OfficeRunning(){return Services.OfficeRunning();}
  public static string OfficeArchitecture(){return OfficeInstallation.Detect();}
  public static string SelectArchitecture(string requested,string detected,bool windows64){return OfficeInstallation.Select(requested,detected,windows64);}
  public static string ExecutableArchitecture(string path){return OfficeInstallation.ExecutableArchitecture(path);}
  public static string RegisteredDll(string architecture){return OfficeInstallation.RegisteredDll(architecture);}
  public static string[] ModulePaths(int processId){return ProcessModules.Paths(processId);}
  public static bool MatchesPayload(string root,string architecture){return OfficeInstallation.MatchesPayload(root,architecture);}
  public static string InspectHosts(){
   var result=new List<object>();string architecture=OfficeArchitecture(),registered=RegisteredDll(architecture);
   string[] hosts={"Word","Excel","PowerPoint"},names={"WINWORD","EXCEL","POWERPNT"};
   for(int index=0;index<hosts.Length;index++){
    bool hasKey=false,enabled=false,loaded=false,readable=true,matches=true;int running=0;var builds=new HashSet<string>();
    using(var key=Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Office\"+hosts[index]+@"\Addins\openOfficetab.Connect")){hasKey=key!=null;enabled=hasKey&&Convert.ToInt32(key.GetValue("LoadBehavior",0))==3;}
    foreach(var process in Process.GetProcessesByName(names[index]))using(process){running++;try{string build=process.MainModule.FileVersionInfo.FileVersion;if(System.Text.RegularExpressions.Regex.IsMatch(build??"",@"\A[0-9.]{3,40}\z"))builds.Add(build);foreach(string module in ModulePaths(process.Id))if(string.Equals(Path.GetFileName(module),"openOfficetab.dll",StringComparison.OrdinalIgnoreCase)){loaded=true;matches&=string.Equals(Path.GetFullPath(module),Path.GetFullPath(registered),StringComparison.OrdinalIgnoreCase);}}catch(System.ComponentModel.Win32Exception){readable=false;}catch(InvalidOperationException){readable=false;}catch(ArgumentException){readable=false;}}
    result.Add(new {name=hosts[index],architecture=architecture,registered=hasKey,enabled=enabled,dllExists=File.Exists(registered),running=running,officeBuilds=builds.OrderBy(b=>b).ToArray(),loaded=loaded,moduleReadable=readable,matchesRegistration=loaded&&matches});
   }return Json.Serialize(result);
  }
  public static string CheckUpdate(string root,bool force){var service=new Services(root);var client=new UpdateClient(service);var release=client.Check(force);return Json.Serialize(new {available=release!=null&&client.IsNewer(release),version=release==null?"":release.Label,currentVersion=client.CurrentIdentity().Text,size=release==null?0L:release.Size,notes=new Dictionary<string,string>{{"zh-CN",release==null?"":release.NotesZh},{"en",release==null?"":release.NotesEn}}});}
  public static string DownloadUpdate(string root,string version){var service=new Services(root);var client=new UpdateClient(service);var release=client.Check(false);if(release==null||release.Label!=version||!client.IsNewer(release))throw new InvalidDataException("Update metadata changed; check again.");return client.Download(release);}
  public static void InstallUpdate(string root,string version){var service=new Services(root);var client=new UpdateClient(service);var release=client.Check(false);if(release==null||release.Label!=version||!client.IsNewer(release))throw new InvalidDataException("Update metadata changed; check again.");client.Launch(release,client.Download(release),true);}
  public static string ParseRelease(string metadata){var release=UpdateClient.Parse(metadata);return Json.Serialize(release==null?null:new {version=release.Label,name=release.Name,hash=release.Hash,size=release.Size});}
  public static bool AllowedDownload(string uri){return UpdateClient.AllowedDownload(new Uri(uri));}
  public static string ProbeRoutes(string root){return Json.Serialize(new UpdateRoutes(root).Probe());}
 }
}
