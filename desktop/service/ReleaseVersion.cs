using System;
using System.Globalization;
using System.Text.RegularExpressions;

namespace OpenOfficeTab.Desktop {
 // A deliberately bounded SemVer subset shared by release parsing and upgrade policy.
 internal sealed class ReleaseVersion : IComparable<ReleaseVersion> {
  internal readonly Version Core;internal readonly string Text,Channel;internal readonly int Number;
  internal bool Prerelease {get{return Channel!="stable";}}
  ReleaseVersion(Version core,string text,string channel,int number){Core=core;Text=text;Channel=channel;Number=number;}
  internal static ReleaseVersion Parse(string text){
   var match=Regex.Match(text??"",@"\A(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,2})\.(0|[1-9][0-9]{0,2})(?:-(alpha|beta|rc)\.([1-9][0-9]{0,5}))?\z");
   if(!match.Success)throw new System.IO.InvalidDataException("Unsupported release version.");
   return new ReleaseVersion(new Version(int.Parse(match.Groups[1].Value,CultureInfo.InvariantCulture),int.Parse(match.Groups[2].Value,CultureInfo.InvariantCulture),int.Parse(match.Groups[3].Value,CultureInfo.InvariantCulture)),text,match.Groups[4].Success?match.Groups[4].Value:"stable",match.Groups[5].Success?int.Parse(match.Groups[5].Value,CultureInfo.InvariantCulture):0);
  }
  static int Rank(string channel){return channel=="alpha"?0:channel=="beta"?1:channel=="rc"?2:3;}
  public int CompareTo(ReleaseVersion other){if(other==null)return 1;int core=Core.CompareTo(other.Core);if(core!=0)return core;int channel=Rank(Channel).CompareTo(Rank(other.Channel));return channel!=0?channel:Number.CompareTo(other.Number);}
  internal static bool Upgrade(string installed,string offered,bool acceptsPrerelease){var next=Parse(offered);return (!next.Prerelease||(acceptsPrerelease&&next.Channel!="alpha"))&&next.CompareTo(Parse(installed))>0;}
 }
}
