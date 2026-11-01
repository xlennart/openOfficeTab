using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Web.Script.Serialization;

// Offline protocol double. It never invokes VMware or records credential args.
static class VmrunStub {
 static string Root=Environment.GetEnvironmentVariable("OOT_VM_TEST_ROOT");
 static string Guest(string path){
  if(path.Contains(".."))throw new Exception("Unsafe fixture path");
  string mapped=Path.GetFullPath(Path.Combine(Root,"guest",path.Replace(':','_').TrimStart('\\').Replace('\\',Path.DirectorySeparatorChar)));
  if(!mapped.StartsWith(Path.GetFullPath(Path.Combine(Root,"guest"))+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))throw new Exception("Fixture escape");
  return mapped;
 }
 static void Json(string path,object value){Directory.CreateDirectory(Path.GetDirectoryName(path));File.WriteAllText(path,new JavaScriptSerializer().Serialize(value));}
 static int Main(string[] args){
  try{
   if(String.IsNullOrEmpty(Root)||!Directory.Exists(Root))throw new Exception("Missing test root");
   if(args.Length==3&&args[2]=="list"){Console.WriteLine("Total running VMs: 1\n"+Environment.GetEnvironmentVariable("OOT_VM_TEST_VMX"));return 0;}
   string op=args[6];string[] p=args.Skip(8).ToArray();File.AppendAllText(Path.Combine(Root,"operations.log"),op+"\n");
   switch(op){
    case "checkToolsState":Console.WriteLine("running");return 0;
    case "fileExistsInGuest":bool exists=File.Exists(Guest(p[0]));Console.WriteLine(exists?"The file exists.":"The file does not exist.");return exists?0:1;
    case "createDirectoryInGuest":if(Directory.Exists(Guest(p[0])))throw new Exception("The directory already exists");Directory.CreateDirectory(Guest(p[0]));return 0;
    case "listDirectoryInGuest":if(!Directory.Exists(Guest(p[0])))throw new Exception("Not a directory");Console.WriteLine("Directory list: 0");return 0;
    case "copyFileFromHostToGuest":File.Copy(p[0],Guest(p[1]),false);return 0;
    case "copyFileFromGuestToHost":File.Copy(Guest(p[0]),p[1],false);return 0;
    case "deleteFileInGuest":File.Delete(Guest(p[0]));return 0;
    case "listProcessesInGuest":Console.WriteLine("Process list: 0");return 0;
    case "runProgramInGuest":
     string action=p[Array.IndexOf(p,"-Action")+1],directory=p[Array.IndexOf(p,"-JobDirectory")+1],job=Path.GetFileName(directory);
     if(action=="Preflight"){Json(Guest(directory+"\\preflight.json"),new{jobId=job,status="complete",unlocked=true,session=1,dpi=96,officeCount=0,centerCount=0});return 0;}
     if(action=="Cleanup"){Directory.Delete(Guest(directory),true);Json(Guest("C:\\OOT-QA\\cleanup-"+job+".json"),new{jobId=job,status="passed"});return 0;}
     throw new Exception("Unsupported fixture action");
    case "stop":if(p.Length!=1||p[0]!="soft")throw new Exception("Only normal shutdown allowed");File.WriteAllText(Path.Combine(Root,"normal-shutdown.txt"),"passed");return 0;
    default:throw new Exception("Unsupported fixture operation");
   }
  }catch(Exception error){Console.WriteLine("Error: "+error.Message);return 1;}
 }
}
