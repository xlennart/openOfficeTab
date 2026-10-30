using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;

namespace OpenOfficeTab.Desktop {
 internal static class DiagnosticLog {
  const int Limit=1024*1024,MaxEvents=4096;
  static JavaScriptSerializer Serializer(){return new JavaScriptSerializer{MaxJsonLength=4*1024*1024};}
  static readonly HashSet<string> Numeric=new HashSet<string>(("schema tick seq targetId outcome runId pid tid host hr droppedRecords activation bits officeMajor officeBuild officeRevision dispid activeId previousId group hwnd viewId saved readOnly hasPath dpi comId nativeId agreement viewCount frameBusy executing refreshing command x y width height fromFrame saveAs independent visible grouped theme language elapsedMs success traceId parentTrace origin jobId api targetHwnd afterHwnd foregroundHwnd activeHwnd flags win32 minimized layoutId requestTrace reason commanded suppressed truncated sizingHwnd operatingHwnd selectedId groupSelectedId hoverId activePainted hoverPainted barHwnd enabled masked").Split(' '));
  static bool Link(string path){return (File.GetAttributes(path)&FileAttributes.ReparsePoint)!=0;}
  internal static object Snapshot(string root){
   var events=new Queue<Dictionary<string,object>>();int skipped=0,read=0;bool truncated=false;
   string directory=Path.Combine(root,"logs");
   try{if(Directory.Exists(directory)&&!Link(directory)){
    var files=new DirectoryInfo(directory).GetFiles("*.jsonl").Where(f=>(f.Name.StartsWith("native-",StringComparison.Ordinal)||f.Name.StartsWith("desktop-",StringComparison.Ordinal))&&!Link(f.FullName)).OrderByDescending(f=>f.LastWriteTimeUtc).ToArray();
    truncated=files.Length>8;
    foreach(var file in files.Take(8).Reverse())try{
     if(file.Length>Limit){skipped++;continue;}
     using(var stream=new FileStream(file.FullName,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))using(var reader=new StreamReader(stream,Encoding.UTF8,true)){
      string line;int bytes=0;while((line=reader.ReadLine())!=null){bytes+=Encoding.UTF8.GetByteCount(line)+1;if(bytes>Limit){truncated=true;break;}
       try{
        if(line.Length>4096)throw new InvalidDataException();
        var raw=Serializer().Deserialize<Dictionary<string,object>>(line);object code,utc,version;
        if(!raw.TryGetValue("event",out code)||!(code is string)||!Regex.IsMatch((string)code,@"\A[a-zA-Z0-9._-]{1,160}\z")||!raw.TryGetValue("utc",out utc)||!(utc is string)||!Regex.IsMatch((string)utc,@"\A\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z\z")||!raw.TryGetValue("version",out version)||!(version is string)||!Regex.IsMatch((string)version,@"\A\d{1,3}\.\d{1,3}\.\d{1,3}(?:-(?:alpha|beta|rc)\.[1-9][0-9]{0,5})?\z"))throw new InvalidDataException();
        var safe=new Dictionary<string,object>{{"event",code},{"utc",utc},{"version",version}};
        foreach(var pair in raw)if(Numeric.Contains(pair.Key)&&(pair.Value is int||pair.Value is long||pair.Value is decimal))safe[pair.Key]=pair.Value;
        events.Enqueue(safe);if(events.Count>MaxEvents){events.Dequeue();truncated=true;}
       }catch{skipped++;}
      }
     }read++;
    }catch(IOException){skipped++;}catch(UnauthorizedAccessException){skipped++;}
   }}catch(IOException){skipped++;}catch(UnauthorizedAccessException){skipped++;}
   return new {schema=1,events=events.OrderBy(e=>(string)e["utc"]).ToArray(),filesRead=read,skippedRecordsOrFiles=skipped,truncated=truncated,maxEvents=MaxEvents,privacy="Event codes and numeric session/window identities only; no document names, paths, content, accounts, URLs or legacy raw logs."};
  }
  internal static void Action(string root,string action,string phase,int hr,long elapsed){
   // Action names originate in the service's fixed ValidateSet, never user text.
   if(!Regex.IsMatch(action??"",@"\A[a-z-]{1,32}\z")||!(phase=="begin"||phase=="end"||phase=="error"))return;
   try{
    string directory=Path.Combine(root,"logs");Directory.CreateDirectory(directory);if(Link(directory))return;
    using(var mutex=new Mutex(false,@"Local\openOfficetab.Log."+StateStore.Identity(root))){bool held=false;try{
     try{held=mutex.WaitOne(0);}catch(AbandonedMutexException){held=true;}if(!held)return;
     string file=Path.Combine(directory,"desktop-current.jsonl");if(File.Exists(file)&&Link(file))return;
     if(File.Exists(file)&&new FileInfo(file).Length>Limit-4096){for(int i=3;i>=1;i--){string old=i==1?file:file+"."+(i-1)+".jsonl",next=file+"."+i+".jsonl";if(File.Exists(next))File.Delete(next);if(File.Exists(old))File.Move(old,next);}}
     var row=new {schema=1,version=Services.DisplayVersion,utc=DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ss.fffZ",CultureInfo.InvariantCulture),pid=System.Diagnostics.Process.GetCurrentProcess().Id,tid=Thread.CurrentThread.ManagedThreadId,host=3,bits=IntPtr.Size*8,hr=hr,elapsedMs=elapsed,@event="service."+action+"."+phase};
     using(var output=new FileStream(file,FileMode.Append,FileAccess.Write,FileShare.Read))using(var writer=new StreamWriter(output,new UTF8Encoding(false)))writer.WriteLine(Serializer().Serialize(row));
    }finally{if(held)mutex.ReleaseMutex();}}
   }catch(IOException){}catch(UnauthorizedAccessException){}
  }
 }
}
