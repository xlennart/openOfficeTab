using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Web.Script.Serialization;

namespace OpenOfficeTab.Desktop {
 internal static class DesktopLanguage {
  [DllImport("kernel32.dll")] static extern ushort GetUserDefaultUILanguage();
  static readonly Dictionary<string,string> English=Load();
  static readonly Dictionary<string,string> Chinese=Reverse();
  static Dictionary<string,string> Load(){using(var stream=typeof(DesktopLanguage).Assembly.GetManifestResourceStream("openOfficetab.DesktopMessages"))using(var reader=new StreamReader(stream))return new JavaScriptSerializer().Deserialize<Dictionary<string,string>>(reader.ReadToEnd());}
  static Dictionary<string,string> Reverse(){var result=new Dictionary<string,string>(StringComparer.Ordinal);foreach(var pair in English)result[pair.Value]=pair.Key;return result;}
  internal static string Resolve(int mode,string systemLocale){return mode==1?"zh-CN":mode==2?"en":(systemLocale??"").StartsWith("zh",StringComparison.OrdinalIgnoreCase)?"zh-CN":"en";}
  internal static object Read(string root){int mode=StateStore.Number(new StateStore(root).Section("openOfficetab"),"language",0);if(mode<0||mode>2)mode=0;string systemLocale=(GetUserDefaultUILanguage()&0x3ff)==4?"zh-CN":"en";return new {mode=mode,locale=Resolve(mode,systemLocale),systemLocale=systemLocale};}
  internal static string Text(string source,string locale){string chinese,translated;if(Chinese.TryGetValue(source,out chinese))source=chinese;return locale=="en"&&English.TryGetValue(source,out translated)?translated:source;}
  internal static string Error(string source,string locale,int hr){if(!English.ContainsKey(source)&&!Chinese.ContainsKey(source))source="操作未完成，请重试或导出诊断与日志。";return Text(source,locale)+" (0x"+unchecked((uint)hr).ToString("X8")+")";}
 }
}
