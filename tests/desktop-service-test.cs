using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
namespace OpenOfficeTab.Desktop {
 internal static class DesktopServiceChecks {
  static int count;static void Check(bool value,string name){if(!value)throw new Exception(name);count++;}
  static string Hash(byte[] bytes){using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-","").ToLowerInvariant();}
  static int Main(string[] args){try{
   string root=Path.GetFullPath(args[0]);Directory.CreateDirectory(root);var store=new StateStore(root);
   using(var process=System.Diagnostics.Process.GetCurrentProcess()){
    string main=process.MainModule.FileName;
    Check(Array.Exists(Api.ModulePaths(process.Id),path=>string.Equals(Path.GetFullPath(path),Path.GetFullPath(main),StringComparison.OrdinalIgnoreCase)),"native module enumeration includes current executable");
   }
   bool unavailable=false;try{Api.ModulePaths(int.MaxValue);}catch(System.ComponentModel.Win32Exception){unavailable=true;}
   Check(unavailable,"unavailable module inspection remains an error");
   foreach(string architecture in new[]{"x86","x64"}) {
    string pe=Path.Combine(root,architecture+".dll");var bytes=new byte[152];
    using(var memory=new MemoryStream(bytes))using(var writer=new BinaryWriter(memory)){writer.Write((ushort)0x5a4d);memory.Position=0x3c;writer.Write(128);memory.Position=128;writer.Write((uint)0x4550);writer.Write((ushort)(architecture=="x86"?0x14c:0x8664));}
    File.WriteAllBytes(pe,bytes);Check(OfficeInstallation.ExecutableArchitecture(pe)==architecture,"PE architecture "+architecture);
    bytes[0]=0;File.WriteAllBytes(pe,bytes);Check(OfficeInstallation.ExecutableArchitecture(pe)=="unknown","invalid DOS signature "+architecture);
    bytes[0]=0x4d;bytes[128]=0;File.WriteAllBytes(pe,bytes);Check(OfficeInstallation.ExecutableArchitecture(pe)=="unknown","invalid PE signature "+architecture);
    File.WriteAllBytes(pe,new byte[5]);Check(OfficeInstallation.ExecutableArchitecture(pe)=="unknown","truncated PE "+architecture);
    Check(OfficeInstallation.Resolve(new[]{architecture,architecture},new[]{architecture})==architecture,"agreeing executable evidence "+architecture);
    Check(OfficeInstallation.Select("auto",architecture,true)==architecture,"automatic payload "+architecture);
    Check(OfficeInstallation.Select(architecture,architecture,true)==architecture,"explicit matching payload "+architecture);
   }
   Check(OfficeInstallation.Resolve(new string[0],new[]{"x86"})=="unknown","registry declaration alone is insufficient");
   Check(OfficeInstallation.Resolve(new[]{"x86","x64"},new string[0])=="conflict","conflicting Office executables");
   Check(OfficeInstallation.Resolve(new[]{"x86"},new[]{"x64"})=="conflict","stale Click-to-Run declaration is rejected");
   Check(OfficeInstallation.Resolve(new[]{"unknown","x64"},new[]{"x64"})=="unknown","unreadable executable cannot be guessed");
   Check(OfficeInstallation.Resolve(new[]{"arm64"},new string[0])=="unsupported","native ARM64 is not x64");
   Check(OfficeInstallation.ExecutableArchitecture(Path.Combine(root,"absent.exe"))=="unknown","missing executable");
   foreach(var input in new[]{new[]{"auto","unknown"},new[]{"auto","conflict"},new[]{"auto","unsupported"},new[]{"x64","x86"},new[]{"x86","x64"},new[]{"invalid","x86"}}){bool rejected=false;try{OfficeInstallation.Select(input[0],input[1],true);}catch(InvalidDataException){rejected=true;}Check(rejected,"invalid architecture evidence or override is rejected");}
   bool windowsRejected=false;try{OfficeInstallation.Select("auto","x86",false);}catch(InvalidDataException){windowsRejected=true;}Check(windowsRejected,"32-bit Windows is outside the unified center target");
   Check(!OfficeInstallation.MatchesPayload(root,"unknown"),"unknown architecture does not inspect a guessed registration");
   store.Merge("openOfficetab",new Dictionary<string,string>{{"schemaVersion","1"},{"tabWidth","120"},{"barColor","#123456"},{"group","0"},{"futureKey","preserve"}});
   var other=new StateStore(root);other.Merge("openOfficetab",new Dictionary<string,string>{{"barColor","#ABCDEF"}});store.Merge("openOfficetab",new Dictionary<string,string>{{"tabWidth","176"}});
   var values=store.Section("openOfficetab");Check(values["barColor"]=="#ABCDEF"&&values["tabWidth"]=="176"&&values["group"]=="0"&&values["futureKey"]=="preserve","merge preserves concurrent and unknown settings");
   store.Merge("desktop",new Dictionary<string,string>{{"autoCheck","1"}});Check(store.Section("openOfficetab")["futureKey"]=="preserve","desktop preferences preserve native section");
   var file=Path.Combine(root,"settings.ini");Check(File.ReadAllBytes(file)[0]==255&&File.ReadAllBytes(file)[1]==254,"native UTF-16 BOM");Check(StateStore.Identity(root)==StateStore.Identity(root.ToUpperInvariant()),"native mutex path identity");
   string metadata="{\"tag_name\":\"v99.0.0\",\"draft\":false,\"prerelease\":false,\"assets\":[{\"name\":\"openOfficetab-99.0.0-setup-x64.exe\",\"browser_download_url\":\"https://github.com/xlennart/openOfficetab/releases/download/v99.0.0/openOfficetab-99.0.0-setup-x64.exe\",\"size\":4}]}";
   byte[] content={77,90,1,2};string hash=Hash(content);int downloads=0;
   var client=new UpdateClient(new Services(root),(url,output,limit)=>{byte[] bytes;if(url.EndsWith("SHA256SUMS.txt"))bytes=Encoding.UTF8.GetBytes(hash+"  openOfficetab-99.0.0-setup-x64.exe\n");else {downloads++;bytes=content;}output.Write(bytes,0,bytes.Length);});
   var release=UpdateClient.Parse(metadata);Check(release.Version==new Version(99,0,0),"parse stable release");
   SignedReleaseMetadata.ReadNotes(release,new Dictionary<string,object>());Check(release.NotesZh==""&&release.NotesEn=="","legacy signed feed without notes remains compatible");
   SignedReleaseMetadata.ReadNotes(release,new Dictionary<string,object>{{"notes",new Dictionary<string,object>{{"zh-CN","更新说明"},{"en","Release notes <script>text only</script>"}}}});Check(release.NotesZh=="更新说明"&&release.NotesEn.Contains("<script>"),"localized signed notes are retained as plain text");
   foreach(object invalid in new object[]{"invalid",new Dictionary<string,object>{{"en",new string('x',6001)}},new Dictionary<string,object>{{"en",42}}}){bool rejected=false;try{SignedReleaseMetadata.ReadNotes(release,new Dictionary<string,object>{{"notes",invalid}});}catch(InvalidDataException){rejected=true;}Check(rejected,"invalid or excessive signed notes rejected");}
   string downloaded=client.Download(release);Check(StateStore.Sha256(downloaded)==hash&&downloads==1,"download checksum and expected cache identity");client.Download(release);Check(downloads==1,"verified cache avoids redundant download");
   File.WriteAllBytes(downloaded,new byte[]{1});client.Download(release);Check(downloads==2&&StateStore.Sha256(downloaded)==hash,"tampered cache is replaced only after verified download");
   var badClient=new UpdateClient(new Services(Path.Combine(root,"bad")),(url,output,limit)=>{var bytes=url.EndsWith("SHA256SUMS.txt")?Encoding.UTF8.GetBytes(hash+"  openOfficetab-99.0.0-setup-x64.exe\n"):new byte[]{1,2,3,4};output.Write(bytes,0,bytes.Length);});
   bool failed=false;try{badClient.Download(UpdateClient.Parse(metadata));}catch(InvalidDataException){failed=true;}Check(failed&&Directory.GetFiles(Path.Combine(root,"bad"),"*.download",SearchOption.AllDirectories).Length==0,"invalid checksum fails and clears partial file");
   foreach(string bad in new[]{metadata.Replace("https://github.com/","https://evil.example/"),metadata.Replace("v99.0.0","v99.0.0-rc1")}){failed=false;try{UpdateClient.Parse(bad);}catch(InvalidDataException){failed=true;}Check(failed,"reject unsupported release metadata");}
   foreach(string url in new[]{"http://github.com/file","https://github.com.evil.example/file","https://github.com:444/file","https://user@github.com/file"})Check(!UpdateClient.AllowedDownload(new Uri(url)),"reject unsafe download origin");
   Check(UpdateClient.Parse(metadata.Replace("\"prerelease\":false","\"prerelease\":true"))==null,"exclude prerelease");
   File.WriteAllText(file,File.ReadAllText(file).Replace("schemaVersion=1","schemaVersion=2"),Encoding.Unicode);string before=StateStore.Sha256(file);failed=false;try{store.Merge("desktop",new Dictionary<string,string>{{"autoCheck","0"}});}catch(InvalidDataException){failed=true;}Check(failed&&before==StateStore.Sha256(file),"unsupported schema leaves original bytes intact");
   string logRoot=Path.Combine(root,"logs");Directory.CreateDirectory(logRoot);
   Api.RecordAction(root,"check-update","begin",0,0);Api.RecordAction(root,"check-update","end",0,123);
   var logJson=Api.DiagnosticTimeline(root);Check(logJson.Contains("service.check-update.begin")&&logJson.Contains("elapsedMs"),"diagnostic timeline contains structured service operation");
   File.WriteAllText(Path.Combine(logRoot,"native-privacy.jsonl"),"{\"event\":\"view.discovered\",\"version\":\"0.2.0\",\"utc\":\"2026-10-28T00:00:00.000Z\",\"viewId\":12,\"documentPath\":\"PRIVATE_MARKER\",\"body\":\"PRIVATE_BODY\"}\ninvalid-json\n");
   logJson=Api.DiagnosticTimeline(root);Check(logJson.Contains("view.discovered")&&!logJson.Contains("PRIVATE_MARKER")&&!logJson.Contains("PRIVATE_BODY")&&!logJson.Contains("documentPath"),"timeline export strips unknown text fields and malformed records");
   File.WriteAllText(Path.Combine(logRoot,"native-oversize.jsonl"),new string('x',1024*1024+1));logJson=Api.DiagnosticTimeline(root);Check(logJson.Length<4*1024*1024&&!logJson.Contains(new string('x',100)),"oversize log excluded from bounded export");
   Console.WriteLine("Desktop service checks passed: "+count);return 0;
  }catch(Exception error){Console.Error.WriteLine(error);return 1;}}
 }
}
