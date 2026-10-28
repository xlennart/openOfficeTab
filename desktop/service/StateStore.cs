using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
namespace OpenOfficeTab.Desktop {
 internal sealed class StateStore {
  internal readonly string Root;
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern uint CharLowerBuff(StringBuilder value,uint length);
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool MoveFileEx(string source,string destination,int flags);
  internal StateStore(string root){Root=Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);}
  internal static string Identity(string path){var text=new StringBuilder(Path.GetFullPath(path).Replace('/','\\'));CharLowerBuff(text,(uint)text.Length);ulong hash=14695981039346656037UL;unchecked{foreach(char c in text.ToString()){hash^=c;hash*=1099511628211UL;}}return hash.ToString("x16");}
  internal Dictionary<string,Dictionary<string,string>> Read(){
   var data=new Dictionary<string,Dictionary<string,string>>(StringComparer.Ordinal);string path=Path.Combine(Root,"settings.ini");if(!File.Exists(path))return data;
   using(var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)){
    if(file.Length>4*1024*1024)throw new InvalidDataException("Settings size limit.");
    using(var reader=new StreamReader(file,new UTF8Encoding(false,true),true)){string line,section="";while((line=reader.ReadLine())!=null){
     if(line.Length==0||line[0]==';'||line[0]=='#')continue;
     if(line[0]=='['&&line.EndsWith("]",StringComparison.Ordinal)){section=line.Substring(1,line.Length-2);if(!data.ContainsKey(section))data[section]=new Dictionary<string,string>(StringComparer.Ordinal);continue;}
     int equal=line.IndexOf('=');if(section.Length>0&&equal>=0)data[section][line.Substring(0,equal)]=line.Substring(equal+1);
    }}
   }return data;
  }
  internal Dictionary<string,string> Section(string name){var data=Read();return data.ContainsKey(name)?data[name]:new Dictionary<string,string>(StringComparer.Ordinal);}
  internal static int Number(Dictionary<string,string> data,string key,int fallback){string text;int value;return data.TryGetValue(key,out text)&&int.TryParse(text,out value)?value:fallback;}
  // Merge only edited keys, holding the same FNV/UTF-16 path mutex as storage.cpp.
  internal void Merge(string section,Dictionary<string,string> changes){
   using(var mutex=new Mutex(false,"Local\\openOfficetab.State."+Identity(Root))){bool held=false;try{
    try{held=mutex.WaitOne(3000);}catch(AbandonedMutexException){held=true;}if(!held)throw new IOException("Settings busy.");
    var data=Read();Dictionary<string,string> native;if(data.TryGetValue("openOfficetab",out native)&&Number(native,"schemaVersion",1)!=1)throw new InvalidDataException("Unsupported settings schema.");
    if(!data.ContainsKey(section))data[section]=new Dictionary<string,string>(StringComparer.Ordinal);
    foreach(var item in changes){if(item.Key.IndexOfAny(new[]{'\r','\n','='})>=0||item.Value.IndexOfAny(new[]{'\r','\n'})>=0)throw new InvalidDataException("Invalid settings value.");data[section][item.Key]=item.Value;}
    if(section=="openOfficetab")data[section]["schemaVersion"]="1";
    var text=new StringBuilder();foreach(var group in data){text.Append('[').Append(group.Key).Append("]\r\n");foreach(var item in group.Value)text.Append(item.Key).Append('=').Append(item.Value).Append("\r\n");}
    Directory.CreateDirectory(Root);string destination=Path.Combine(Root,"settings.ini"),temp=destination+"."+Guid.NewGuid().ToString("N")+".tmp";
    try{using(var stream=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)){byte[] bom=Encoding.Unicode.GetPreamble(),bytes=Encoding.Unicode.GetBytes(text.ToString());stream.Write(bom,0,bom.Length);stream.Write(bytes,0,bytes.Length);stream.Flush(true);}if(!MoveFileEx(temp,destination,1|8))throw new IOException("Atomic settings write failed.");}finally{if(File.Exists(temp))File.Delete(temp);}
   }finally{if(held)mutex.ReleaseMutex();}}
  }
  internal static string Sha256(string path){using(var sha=SHA256.Create())using(var stream=File.OpenRead(path))return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-","").ToLowerInvariant();}
 }
}
