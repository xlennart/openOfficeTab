using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace OpenOfficeTab.Desktop {
 internal sealed class ReleaseInfo {internal Version Version;internal string Tag,Name,Url,Hash,NotesZh="",NotesEn="";internal long Size;}
 internal sealed class UpdateClient {
  readonly Services services;readonly Action<string,Stream,long> fetch;readonly UpdateRoutes routes;
  internal UpdateClient(Services value){services=value;routes=new UpdateRoutes(value.Store.Root);fetch=routes.Fetch;}
  internal UpdateClient(Services value,Action<string,Stream,long> transport){services=value;fetch=transport;}
  internal static bool AllowedDownload(Uri uri){return uri.Scheme=="https"&&uri.Port==443&&uri.UserInfo.Length==0&&Array.IndexOf(new[]{"api.github.com","github.com","release-assets.githubusercontent.com","objects.githubusercontent.com"},uri.DnsSafeHost)>=0;}
  // Explicit redirect allowlist, bounded streams, no tokens or cookies.
  static void Fetch(string url,Stream target,long limit){
   var uri=new Uri(url);ServicePointManager.SecurityProtocol|=SecurityProtocolType.Tls12;
   for(int hop=0;hop<6;hop++){
    if(!AllowedDownload(uri))throw new InvalidDataException("Unexpected download host.");
    var request=(HttpWebRequest)WebRequest.Create(uri);request.AllowAutoRedirect=false;request.UserAgent="openOfficetab/"+Services.Version;request.Accept="application/vnd.github+json";request.Timeout=15000;request.ReadWriteTimeout=30000;
    using(var response=(HttpWebResponse)request.GetResponse()){
     int code=(int)response.StatusCode;if(code>=300&&code<400){uri=new Uri(uri,response.Headers["Location"]);continue;}
     if(code!=200||response.ContentLength>limit)throw new InvalidDataException("Unexpected download response.");
     using(var input=response.GetResponseStream()){var bytes=new byte[65536];long count=0;int read;while((read=input.Read(bytes,0,bytes.Length))>0){count+=read;if(count>limit)throw new InvalidDataException("Download exceeds size limit.");target.Write(bytes,0,read);}}return;
    }
   }throw new InvalidDataException("Too many redirects.");
  }
  string FetchText(string url,long limit){using(var memory=new MemoryStream()){fetch(url,memory,limit);return new UTF8Encoding(false,true).GetString(memory.ToArray());}}
  internal static ReleaseInfo Parse(string json){
   var serializer=new JavaScriptSerializer{MaxJsonLength=2*1024*1024};var data=serializer.Deserialize<Dictionary<string,object>>(json);
   if(data==null||!data.ContainsKey("tag_name")||!data.ContainsKey("draft")||!data.ContainsKey("prerelease")||!data.ContainsKey("assets"))throw new InvalidDataException("Incomplete release metadata.");
   if((bool)data["draft"]||(bool)data["prerelease"])return null;
   string tag=(string)data["tag_name"];if(!Regex.IsMatch(tag,"^v?[0-9]+\\.[0-9]+\\.[0-9]+$"))throw new InvalidDataException("Unsupported release tag.");
   Version version;if(!Version.TryParse(tag.TrimStart('v'),out version))throw new InvalidDataException("Invalid release version.");
   string name="openOfficetab-"+version+"-setup-x64.exe",expected=DesktopProgram.Repo+"/releases/download/"+tag+"/"+name;
   foreach(var item in (IEnumerable)data["assets"]){var asset=(Dictionary<string,object>)item;if((string)asset["name"]!=name)continue;
    string url=(string)asset["browser_download_url"];if(url!=expected)throw new InvalidDataException("Unexpected installer URL.");long size=Convert.ToInt64(asset["size"]);if(size<=0||size>80*1024*1024)throw new InvalidDataException("Unexpected installer size.");
    string hash=asset.ContainsKey("digest")?asset["digest"] as string:null;if(hash!=null&&!Regex.IsMatch(hash,"^sha256:[0-9a-fA-F]{64}$"))throw new InvalidDataException("Invalid installer digest.");
    return new ReleaseInfo{Version=version,Tag=tag,Name=name,Url=url,Size=size,Hash=hash==null?null:hash.Substring(7).ToLowerInvariant()};
   }throw new InvalidDataException("No compatible Windows installer.");
  }
  internal ReleaseInfo Check(bool force){
   if(services.Preview)throw new InvalidOperationException("Preview does not contact update servers.");
   if(routes!=null){if(force)routes.Probe();var signed=routes.SignedRelease();return signed??UpdateRoutes.LegacyCurrent();}
   string cache=Path.Combine(services.Store.Root,"updates","latest.json"),json=null;
   if(!force&&File.Exists(cache)&&DateTime.UtcNow-File.GetLastWriteTimeUtc(cache)<TimeSpan.FromHours(6))json=File.ReadAllText(cache,Encoding.UTF8);
   if(json==null){json=FetchText("https://api.github.com/repos/xlennart/openOfficetab/releases/latest",2*1024*1024);var checkedRelease=Parse(json);Directory.CreateDirectory(Path.GetDirectoryName(cache));File.WriteAllText(cache,json,new UTF8Encoding(false));return checkedRelease;}
   return Parse(json);
  }
  internal Version CurrentVersion(){Version installed=Services.Version;string value=(string)Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\openOfficetab","DesktopVersion",Services.Version.ToString());Version current;if(Version.TryParse(value,out current)&&current>installed)installed=current;return installed;}
  internal bool IsNewer(ReleaseInfo release){return release.Version>CurrentVersion();}
  internal string Download(ReleaseInfo release){
   if(services.Preview)throw new InvalidOperationException("Preview cannot download updates.");
   if(release.Hash==null){string sums=FetchText(DesktopProgram.Repo+"/releases/download/"+release.Tag+"/SHA256SUMS.txt",65536);foreach(string line in sums.Split('\n')){var match=Regex.Match(line.TrimEnd('\r'),"^([0-9a-fA-F]{64})  "+Regex.Escape(release.Name)+"$");if(match.Success)release.Hash=match.Groups[1].Value.ToLowerInvariant();}if(release.Hash==null)throw new InvalidDataException("No installer checksum.");}
   string directory=Path.Combine(services.Store.Root,"updates",release.Version+"-"+release.Hash.Substring(0,12)),file=Path.Combine(directory,"setup.exe");Directory.CreateDirectory(directory);
   if(File.Exists(file)&&StateStore.Sha256(file)==release.Hash)return file;
   string temp=Path.Combine(directory,Guid.NewGuid().ToString("N")+".download");
   try{using(var stream=new FileStream(temp,FileMode.CreateNew,FileAccess.ReadWrite,FileShare.None)){if(routes!=null)routes.FetchVerifiedAsset(release.Url,stream,80*1024*1024,release.Size,release.Hash);else fetch(release.Url,stream,80*1024*1024);stream.Flush(true);}if(new FileInfo(temp).Length!=release.Size||StateStore.Sha256(temp)!=release.Hash)throw new InvalidDataException("Installer checksum mismatch.");if(File.Exists(file))File.Delete(file);File.Move(temp,file);return file;}finally{if(File.Exists(temp))File.Delete(temp);}
  }
  internal void Launch(ReleaseInfo release,string file,bool silent){
   if(services.Preview||Services.OfficeRunning())throw new InvalidOperationException("Exit Office first.");
   if(!IsNewer(release))return;
   string expected=Path.Combine(services.Store.Root,"updates",release.Version+"-"+release.Hash.Substring(0,12),"setup.exe");
   if(!string.Equals(Path.GetFullPath(file),expected,StringComparison.OrdinalIgnoreCase)||StateStore.Sha256(file)!=release.Hash)throw new InvalidDataException("Installer identity changed.");
   using(var process=Process.Start(new ProcessStartInfo(file,silent?"--silent":""){UseShellExecute=false,CreateNoWindow=silent})){
    if(!process.WaitForExit(120000))throw new TimeoutException("Update is still running; inspect status before retrying.");
    if(process.ExitCode!=0)throw new InvalidOperationException("Update installation failed.");
   }
   string installed=(string)Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\openOfficetab","DesktopVersion","");
   if(installed!=release.Version.ToString())throw new InvalidDataException("Installed version could not be verified.");
  }
 }
}
