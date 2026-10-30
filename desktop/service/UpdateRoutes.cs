using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace OpenOfficeTab.Desktop {
 internal sealed class OwnedUpdateSource {public string name=null,feedUrl=null,releaseBase=null,probeUrl=null,latestApi=null;public string[] redirectHosts=null;}
 internal sealed class UpdateSourceConfig {public OwnedUpdateSource[] ownedSources=null;}
 internal sealed class RouteHealth {
  public string name;public bool available;public long latencyMs;public string status;
 }
 internal sealed class UpdateRoutes {
  internal static readonly string[] Names={"GitHub","ghfast.top","ghproxy.com","mirror.ghproxy.com"};
  static readonly string[] Prefixes={"","https://ghfast.top/","https://ghproxy.com/","https://mirror.ghproxy.com/"};
  static readonly OwnedUpdateSource[] Owned=LoadOwned();
  static OwnedUpdateSource[] LoadOwned(){
   using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("openOfficetab.UpdateSources"))using(var reader=new StreamReader(stream)){
    var values=new JavaScriptSerializer().Deserialize<UpdateSourceConfig>(reader.ReadToEnd()).ownedSources??new OwnedUpdateSource[0];
    if(values.Length>4)throw new InvalidDataException("Too many configured update sources.");
    foreach(var value in values){if(string.IsNullOrEmpty(value.name)||value.name.Length>40)throw new InvalidDataException("Invalid source name.");foreach(string url in new[]{value.feedUrl,value.releaseBase,value.probeUrl}){var uri=new Uri(url);if(uri.Scheme!="https"||uri.Port!=443||uri.UserInfo.Length!=0)throw new InvalidDataException("Owned update sources require HTTPS.");}}
    return values;
   }
  }
  static int SourceCount {get{return Names.Length+Owned.Length;}}
  static string SourceName(int index){return index<Names.Length?Names[index]:Owned[index-Names.Length].name;}
  readonly string root;internal UpdateRoutes(string directory){root=directory;}
  internal static bool Allowed(Uri uri){return UpdateClient.AllowedDownload(uri)||uri.Scheme=="https"&&uri.Port==443&&uri.UserInfo.Length==0&&(Names.Skip(1).Contains(uri.DnsSafeHost)||Owned.Any(source=>new[]{source.feedUrl,source.releaseBase,source.probeUrl,source.latestApi}.Where(url=>!string.IsNullOrEmpty(url)).Any(url=>new Uri(url).DnsSafeHost==uri.DnsSafeHost)||(source.redirectHosts??new string[0]).Contains(uri.DnsSafeHost)));}
  static HttpWebResponse Open(string url,string method){
   ServicePointManager.SecurityProtocol|=SecurityProtocolType.Tls12;var uri=new Uri(url);
   for(int hop=0;hop<6;hop++){
    if(!Allowed(uri))throw new InvalidDataException("Download redirect is outside the configured routes.");
    var request=(HttpWebRequest)WebRequest.Create(uri);request.AllowAutoRedirect=false;request.Method=method;request.UserAgent="openOfficetab/0.2.2";request.Timeout=4000;request.ReadWriteTimeout=5000;
    var response=(HttpWebResponse)request.GetResponse();int status=(int)response.StatusCode;
    if(status>=300&&status<400){string next=response.Headers["Location"];response.Dispose();uri=new Uri(uri,next);continue;}return response;
   }throw new InvalidDataException("Too many download redirects.");
  }
  internal static void Direct(string url,Stream target,long limit){
   using(var response=Open(url,"GET")){
    if((int)response.StatusCode!=200||response.ContentLength>limit)throw new InvalidDataException("Unexpected download response.");
    using(var input=response.GetResponseStream()){byte[] buffer=new byte[65536];long total=0;int length;var timer=Stopwatch.StartNew();while((length=input.Read(buffer,0,buffer.Length))>0){total+=length;if(total>limit||timer.Elapsed>TimeSpan.FromMinutes(3))throw new InvalidDataException("Download size or time limit exceeded.");target.Write(buffer,0,length);}}
   }
  }
  static byte[] Read(string url,long limit){using(var data=new MemoryStream()){Direct(url,data,limit);return data.ToArray();}}
  static bool ReleaseUrl(string url){return url.StartsWith(DesktopProgram.Repo+"/releases/download/",StringComparison.Ordinal)||url.StartsWith(DesktopProgram.Repo+"/releases/latest/download/",StringComparison.Ordinal);}
  internal static string Candidate(string original,int index){
   if(!ReleaseUrl(original)||index<0||index>=SourceCount)throw new InvalidDataException("Unsupported release route.");
   if(index<Names.Length)return Prefixes[index]+original;
   var source=Owned[index-Names.Length];string marker="/releases/download/";int position=original.IndexOf(marker,StringComparison.Ordinal);
   return position<0?OwnedLatest(source,"openofficetab-update.json"):source.releaseBase.TrimEnd('/')+"/"+original.Substring(position+marker.Length);
  }
  static string OwnedLatest(OwnedUpdateSource source,string filename){
   if(string.IsNullOrEmpty(source.latestApi))return filename=="SHA256SUMS.txt"?source.probeUrl:source.feedUrl;
   var data=new JavaScriptSerializer().Deserialize<Dictionary<string,object>>(new UTF8Encoding(false,true).GetString(Read(source.latestApi,65536)));
   string tag=data["tag_name"] as string;
   if(tag==null||!Regex.IsMatch(tag,"^v?[0-9]+\\.[0-9]+\\.[0-9]+$"))throw new InvalidDataException("Invalid mirror release tag.");
   return source.releaseBase.TrimEnd('/')+"/"+tag+"/"+filename;
  }
  internal RouteHealth[] Probe(){
   const string bootstrap="https://github.com/xlennart/openOfficetab/releases/download/v0.1.9/SHA256SUMS.txt";
   var tasks=Enumerable.Range(0,SourceCount).Select(index=>Task.Run(()=>{
    var result=new RouteHealth{name=SourceName(index)};var timer=Stopwatch.StartNew();
    try{string text=new UTF8Encoding(false,true).GetString(Read(index<Names.Length?Candidate(bootstrap,index):OwnedLatest(Owned[index-Names.Length],"SHA256SUMS.txt"),65536)).Replace("\r","");result.available=Regex.IsMatch(text,"(?m)^[0-9a-fA-F]{64}  openOfficetab-[^\\r\\n]+$");result.status=result.available?"内容探测通过":"响应不是有效发布校验清单";}
    catch(WebException error){result.status=error.Response is HttpWebResponse?"HTTP "+(int)((HttpWebResponse)error.Response).StatusCode:"连接失败或超时";}
    catch(Exception){result.status="响应内容无效";}result.latencyMs=timer.ElapsedMilliseconds;return result;
   })).ToArray();Task.WaitAll(tasks);var results=tasks.Select(task=>task.Result).ToArray();
   Directory.CreateDirectory(Path.Combine(root,"updates"));File.WriteAllText(Path.Combine(root,"updates","routes.json"),new JavaScriptSerializer().Serialize(results),new UTF8Encoding(false));return results;
  }
  int[] Order(){
   string file=Path.Combine(root,"updates","routes.json");RouteHealth[] health=null;
   try{if(File.Exists(file)&&DateTime.UtcNow-File.GetLastWriteTimeUtc(file)<TimeSpan.FromMinutes(10))health=new JavaScriptSerializer().Deserialize<RouteHealth[]>(File.ReadAllText(file));}catch(Exception){}
   if(health==null)health=Probe();
   return Enumerable.Range(0,SourceCount).OrderBy(index=>{var status=health.FirstOrDefault(item=>item.name==SourceName(index));return status!=null&&status.available?status.latencyMs:long.MaxValue;}).ToArray();
  }
  internal void Fetch(string url,Stream target,long limit){
   if(!ReleaseUrl(url)){Direct(url,target,limit);return;}
   Exception last=null;
   foreach(int index in Order())try{target.SetLength(0);target.Position=0;Direct(Candidate(url,index),target,limit);return;}catch(Exception error){last=error;}
   throw new IOException("所有更新线路均未完成下载，请重新检测线路。",last);
  }
  internal void FetchVerifiedAsset(string url,Stream target,long limit,long size,string hash){
   Exception last=null;
   foreach(int index in Order())try{
    target.SetLength(0);target.Position=0;Direct(Candidate(url,index),target,limit);
    if(target.Length!=size)throw new InvalidDataException("Installer length mismatch.");
    target.Position=0;string actual;using(var sha=SHA256.Create())actual=BitConverter.ToString(sha.ComputeHash(target)).Replace("-","").ToLowerInvariant();
    if(actual!=hash)throw new InvalidDataException("Installer checksum mismatch.");target.Position=target.Length;return;
   }catch(Exception error){last=error;}
   throw new IOException("各线路下载均未通过校验，未执行安装。",last);
  }
  internal ReleaseInfo SignedRelease(){
   string feed=DesktopProgram.Repo+"/releases/latest/download/openofficetab-update.json";
   var tasks=Enumerable.Range(0,SourceCount).Select(index=>Task.Run(()=>{
    try{return SignedReleaseMetadata.Parse(new UTF8Encoding(false,true).GetString(Read(Candidate(feed,index),65536)));}catch(Exception){return null;}
   })).ToArray();Task.WaitAll(tasks);
   var releases=tasks.Select(task=>task.Result).Where(release=>release!=null).ToList();
   // Only the user's Beta channel preference opts into discovery. API data is untrusted: it
   // can nominate tags, but every candidate still needs the existing RSA trust anchor.
   if(new Services(root).AcceptsPrerelease)try{
    var list=new JavaScriptSerializer().DeserializeObject(new UTF8Encoding(false,true).GetString(Read("https://api.github.com/repos/xlennart/openOfficeTab/releases?per_page=20",2*1024*1024))) as object[];
    if(list!=null)foreach(var item in list.OfType<Dictionary<string,object>>().Where(item=>item.ContainsKey("draft")&&item["draft"] is bool&&!(bool)item["draft"]&&item.ContainsKey("prerelease")&&item["prerelease"] is bool&&(bool)item["prerelease"]).Take(6))try{
     string tag=item["tag_name"] as string;var identity=ReleaseVersion.Parse(tag!=null&&tag.StartsWith("v",StringComparison.Ordinal)?tag.Substring(1):tag);if(!identity.Prerelease||identity.Channel=="alpha")continue;
     string beta=DesktopProgram.Repo+"/releases/download/"+tag+"/openofficetab-update-beta.json";
     var candidates=Enumerable.Range(0,SourceCount).Select(index=>Task.Run(()=>{try{return SignedReleaseMetadata.Parse(new UTF8Encoding(false,true).GetString(Read(Candidate(beta,index),65536)),true);}catch(Exception){return null;}})).ToArray();Task.WaitAll(candidates);
     var candidate=candidates.Select(task=>task.Result).FirstOrDefault(value=>value!=null&&value.Tag==tag&&value.Label==identity.Text);if(candidate==null)continue;
     if(candidate.Tag!=tag||candidate.Label!=identity.Text)throw new InvalidDataException("Prerelease nomination mismatch.");releases.Add(candidate);
    }catch(Exception){}
   }catch(Exception){}
   return releases.OrderByDescending(release=>release.Identity??ReleaseVersion.Parse(release.Version.ToString())).FirstOrDefault();
  }
  internal static ReleaseInfo LegacyCurrent(){
   using(var response=Open(DesktopProgram.Repo+"/releases/latest","HEAD")){
    string prefix=DesktopProgram.Repo+"/releases/tag/",uri=response.ResponseUri.GetLeftPart(UriPartial.Path);
    if(!uri.StartsWith(prefix,StringComparison.Ordinal))throw new InvalidDataException("Unexpected latest release page.");
    string tag=uri.Substring(prefix.Length);Version version;
    if(!Regex.IsMatch(tag,"^v?[0-9]+\\.[0-9]+\\.[0-9]+$")||!Version.TryParse(tag.TrimStart('v'),out version)||version>Services.Version)throw new InvalidDataException("新版本尚无可验证更新清单，请查看 GitHub 发布说明。");
    return new ReleaseInfo{Version=version,Tag=tag};
   }
  }
 }
 internal static class SignedReleaseMetadata {
  internal static void ReadNotes(ReleaseInfo release,Dictionary<string,object> data){
   if(!data.ContainsKey("notes"))return;
   var notes=data["notes"] as Dictionary<string,object>;if(notes==null)throw new InvalidDataException("Invalid signed release notes.");
   foreach(string locale in new[]{"zh-CN","en"})if(notes.ContainsKey(locale)){
    var text=notes[locale] as string;if(text==null||text.Length>6000||text.IndexOf('\0')>=0)throw new InvalidDataException("Invalid signed release notes.");
    if(locale=="zh-CN")release.NotesZh=text;else release.NotesEn=text;
   }
  }
  internal static ReleaseInfo Parse(string envelope,bool acceptsPrerelease=false){
   var serializer=new JavaScriptSerializer{MaxJsonLength=65536};var signed=serializer.Deserialize<Dictionary<string,string>>(envelope);
   byte[] payload=Convert.FromBase64String(signed["payload"]),signature=Convert.FromBase64String(signed["signature"]);
   string publicKey;using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("openOfficetab.UpdatePublicKey"))using(var reader=new StreamReader(stream))publicKey=reader.ReadToEnd();
   using(var rsa=new RSACryptoServiceProvider()){rsa.PersistKeyInCsp=false;rsa.FromXmlString(publicKey);if(!rsa.VerifyData(payload,CryptoConfig.MapNameToOID("SHA256"),signature))throw new InvalidDataException("更新清单签名无效。");}
   var data=serializer.Deserialize<Dictionary<string,object>>(new UTF8Encoding(false,true).GetString(payload));
   if(Convert.ToInt32(data["schemaVersion"])!=1||(string)data["protocol"]!="openofficetab-desktop-v1"||(string)data["architecture"]!="x64")throw new InvalidDataException("Unsupported update protocol.");
   var asset=new Dictionary<string,object>{{"name",data["name"]},{"browser_download_url",data["url"]},{"size",data["size"]},{"digest","sha256:"+(string)data["sha256"]}};
   var identity=ReleaseVersion.Parse((string)data["version"]);
   if((identity.Prerelease&&!data.ContainsKey("channel"))||(data.ContainsKey("channel")&&(string)data["channel"]!=identity.Channel))throw new InvalidDataException("Inconsistent signed release channel.");
   var metadata=new Dictionary<string,object>{{"tag_name",data["tag"]},{"draft",false},{"prerelease",identity.Prerelease},{"assets",new[]{asset}}};
   var release=UpdateClient.Parse(serializer.Serialize(metadata),acceptsPrerelease);if(release==null)return null;if(release.Label!=identity.Text)throw new InvalidDataException("Inconsistent signed version.");ReadNotes(release,data);return release;
  }
 }
}
