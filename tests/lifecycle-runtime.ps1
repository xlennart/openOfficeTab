param([ValidateSet('Word','Excel','PowerPoint')][string]$HostName='Word',[switch]$UseExistingFixture,[string]$ExpectedVersion='0.2.3-beta.1',[switch]$DeferProcessExitCheck)
$ErrorActionPreference='Stop'
if($env:CODEX_WINDOWS_SANDBOX_PACKAGE_FAMILY){throw 'Run outside the development process context.'}
$taskRoot=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'common.ps1')
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
[ComVisible(true),Guid("00020A01-0000-0000-C000-000000000046"),InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
public interface IWordCancel {[DispId(6)] void BeforeClose([MarshalAs(UnmanagedType.IDispatch)] object doc,[MarshalAs(UnmanagedType.VariantBool)] ref bool cancel);}
[ComVisible(true),Guid("00024413-0000-0000-C000-000000000046"),InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
public interface IExcelCancel {[DispId(1570)] void BeforeClose([MarshalAs(UnmanagedType.IDispatch)] object doc,[MarshalAs(UnmanagedType.VariantBool)] ref bool cancel);}
[ComVisible(true),Guid("914934C2-5A91-11CF-8700-00AA0060263B"),InterfaceType(ComInterfaceType.InterfaceIsIDispatch)]
public interface IPptCancel {[DispId(2025)] void BeforeClose([MarshalAs(UnmanagedType.IDispatch)] object doc,[MarshalAs(UnmanagedType.VariantBool)] ref bool cancel);}
public class NativeCloseCancel : IDisposable {
 IConnectionPoint point;int cookie;public bool Armed;public int Cancelled;
 protected NativeCloseCancel(object app,string guid){Guid id=new Guid(guid);((IConnectionPointContainer)app).FindConnectionPoint(ref id,out point);point.Advise(this,out cookie);}
 public void BeforeClose(object doc,ref bool cancel){if(Armed){cancel=true;Cancelled++;}}
 public void Dispose(){if(point!=null){point.Unadvise(cookie);Marshal.ReleaseComObject(point);point=null;}}
}
[ComVisible(true),ClassInterface(ClassInterfaceType.None),ComDefaultInterface(typeof(IWordCancel))]
public sealed class WordCloseCancel : NativeCloseCancel,IWordCancel {public WordCloseCancel(object app):base(app,"00020A01-0000-0000-C000-000000000046") {}}
[ComVisible(true),ClassInterface(ClassInterfaceType.None),ComDefaultInterface(typeof(IExcelCancel))]
public sealed class ExcelCloseCancel : NativeCloseCancel,IExcelCancel {public ExcelCloseCancel(object app):base(app,"00024413-0000-0000-C000-000000000046") {}}
[ComVisible(true),ClassInterface(ClassInterfaceType.None),ComDefaultInterface(typeof(IPptCancel))]
public sealed class PowerPointCloseCancel : NativeCloseCancel,IPptCancel {public PowerPointCloseCancel(object app):base(app,"914934C2-5A91-11CF-8700-00AA0060263B") {}}
public static class TestWindows {
 [StructLayout(LayoutKind.Sequential)]public struct Point {public int X,Y;}
 [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr w,int command);
 [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr w);
 [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr w,IntPtr after,int x,int y,int width,int height,uint flags);
 [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point p);
 [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr w,uint flags);
 [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 public static void Front(long w){var window=new IntPtr(w);SetWindowPos(window,IntPtr.Zero,0,0,0,0,0x13);SetForegroundWindow(window);}
 public static void ExposeDropTarget(long w,bool above){SetWindowPos(new IntPtr(w),new IntPtr(above?-1:-2),0,0,0,0,0x13);}
 public static object RootAt(int x,int y){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{return GetAncestor(WindowFromPoint(new Point{X=x,Y=y}),2).ToInt64();}finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}}

 [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr window);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
}
'@
function Wait-State([scriptblock]$check){
    $deadline=[DateTime]::UtcNow.AddSeconds(8)
    do{
        try{$script:api.Refresh()|Out-Null;$state=$script:api.GetState()|ConvertFrom-Json;if(& $check $state){return $state}}
        catch{if($_.Exception.HResult -notin @(-2147418111,-2147417846) -and $_.Exception.InnerException.HResult -notin @(-2147418111,-2147417846)){throw}}
        Start-Sleep -Milliseconds 40
    }while([DateTime]::UtcNow -lt $deadline)
    $caller=(Get-PSCallStack)[1].ScriptLineNumber
    throw ('Runtime state did not converge at line '+$caller+'; test documents retained. State: '+($state|ConvertTo-Json -Depth 8 -Compress))
}
function Set-TestSaved($document,[bool]$saved){
    if($HostName -eq 'PowerPoint'){
        if($saved){
            for($attempt=0;$attempt -lt 6;$attempt++){$document.Saved=-1;Start-Sleep -Milliseconds 150;if($document.Saved -eq -1){return}}
            throw 'Generated PowerPoint document is still changing; preserved.'
        }else{$document.Saved=0}
    }
    else{$document.Saved=$saved}
}
$processName=switch($HostName){'Word'{'WINWORD'};'Excel'{'EXCEL'};'PowerPoint'{'POWERPNT'}}
if(-not $UseExistingFixture -and @(Get-Process -Name $processName -ErrorAction SilentlyContinue).Count){throw 'An existing Office process was preserved; close it before running this isolated fixture test.'}
$fixtureName=switch($HostName){'Word'{'word-demo.docx'};'Excel'{'excel-demo.xlsx'};'PowerPoint'{'powerpoint-demo.pptx'}}
$fixturePath=Join-Path $taskRoot ('tests\fixtures\'+$fixtureName)
if(-not $UseExistingFixture){Start-Process -FilePath $fixturePath -WindowStyle Normal}
$fixture=$null;$app=$null;$api=$null;$hook=$null
$deadline=[DateTime]::UtcNow.AddSeconds(12)
do{try{if($HostName -eq 'PowerPoint'){$running=[OfficeRot]::Attach('PowerPoint');$fixture=$running.Presentations.Item(1);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($running);$running=$null}else{$fixture=[OfficeRot]::AttachFile($fixturePath)}}catch{Start-Sleep -Milliseconds 100}}while(-not $fixture -and [DateTime]::UtcNow -lt $deadline)
if(-not $fixture -or $fixture.FullName -ne $fixturePath){throw 'Exact fixture not found; all open documents preserved.'}
$app=$fixture.Application
switch($HostName){'Word'{$collection=$app.Documents};'Excel'{$collection=$app.Workbooks};'PowerPoint'{$collection=$app.Presentations}}
if($collection.Count -ne 1){throw 'Other documents detected; all documents preserved.'}
if(-not $fixture.Saved){throw 'Fixture has edits; preserved for inspection.'}
$api=$app.COMAddIns.Item('openOfficetab.Connect').Object
$before=Wait-State {param($s)$s.views.Count -eq 1 -and $s.paneMetrics[0].displayVisible}
if($before.version -ne $ExpectedVersion){throw 'Wrong add-in version.'}
$baseId=$before.views[0].id;$baseRoot=$before.views[0].hwnd
[uint32]$officePid=0
[void][TestWindows]::GetWindowThreadProcessId([IntPtr]$baseRoot,[ref]$officePid)
$officeStartUtc=(Get-Process -Id $officePid).StartTime.ToUniversalTime().ToString('o')
$originalWidth=$before.diagnostics.tabWidth;$originalMode=$before.diagnostics.widthMode
try{
    $hookType=$HostName+'CloseCancel'
    $hook=New-Object $hookType -ArgumentList $app
    $hook.Armed=$true
    $closeRequest=$api.Close($baseId)
    if($HostName -ne 'PowerPoint' -and $closeRequest){throw 'Native close cancellation was ignored.'}
    $cancelled=Wait-State {param($s)$s.views.Count -eq 1 -and $s.views[0].id -eq $baseId -and -not $s.diagnostics.closePending}
    if($hook.Cancelled -lt 1){throw 'Native cancellation event was not received.'}
    $hook.Armed=$false
    if(-not $api.SetTabWidth(200,0)){throw 'Width change failed.'}
    if($api.SetTabWidth(79,0) -or $api.SetTabWidth(361,0) -or $api.SetTabWidth(120,3)){throw 'Invalid width accepted.'}
    $widthState=Wait-State {param($s)$s.diagnostics.tabWidth -eq 200 -and $s.diagnostics.widthMode -eq 0}
    $api.NewDocument()
    $added=Wait-State {param($s)$s.views.Count -eq 2 -and $s.activeId -ne $baseId}
    $newId=($added.views|Where-Object id -ne $baseId).id
    $newRoot=($added.views|Where-Object id -eq $newId).hwnd
    $api.Activate($newId)
    $activated=Wait-State {param($s)$s.activeId -eq $newId}
    $created=switch($HostName){'Word'{$app.ActiveDocument};'Excel'{$app.ActiveWorkbook};'PowerPoint'{$app.ActivePresentation}}
    $marker='PRIVATE_TEST_MARKER_not_for_diagnostics'
    switch($HostName){'Word'{$created.Content.Text=$marker};'Excel'{$created.Worksheets.Item(1).Range('A1').Value2=$marker};'PowerPoint'{$created.Slides.Item(1).Shapes.Item(1).TextFrame.TextRange.Text=$marker}}
    if(-not $api.NewTabGroup($newId)){throw 'New tab group failed.'}
    $split=Wait-State {param($s)@($s.views.groupId|Select-Object -Unique).Count -eq 2}
    $newGroup=($split.views|Where-Object id -eq $newId).groupId
    if(($split.views|Where-Object id -eq $newId).hwnd -ne $newRoot -or $created.Saved){throw 'Split replaced or saved the document.'}
    $api.Activate($baseId)
    $baseActivated=Wait-State {param($s)$s.activeId -eq $baseId}
    $api.SaveAll();$api.CloseOther($baseId)
    $excluded=Wait-State {param($s)$s.views.Count -eq 2}
    if($created.Saved){throw 'Group save touched another group.'}
    $api.NewIndependentDocument()
    $independent=Wait-State {param($s)$s.views.Count -eq 3 -and @($s.views|Where-Object independent).Count -eq 1 -and $s.activeId -eq ($s.views|Where-Object independent).id}
    $independentId=($independent.views|Where-Object independent).id
    $api.Activate($independentId)
    $activated=Wait-State {param($s)$s.activeId -eq $independentId}
    $extra=switch($HostName){'Word'{$app.ActiveDocument};'Excel'{$app.ActiveWorkbook};'PowerPoint'{$app.ActivePresentation}}
    $independentPane=$independent.paneMetrics|Where-Object viewId -eq $independentId
    if($independentPane.officeVisible -or $independentPane.displayVisible){throw 'Independent new document exposed a tab bar.'}
    if(-not $api.MoveToGroup($independentId,$newGroup)){throw 'Join target group failed.'}
    $joined=Wait-State {param($s)($s.views|Where-Object id -eq $independentId).groupId -eq $newGroup}
    $hook.Armed=$true
    # PowerPoint displays its unsaved prompt before PresentationBeforeClose.
    # Avoid automating that dialog: cancel the native event with the test-only
    # Saved flag temporarily set, then restore unsaved status and verify content.
    if($HostName -eq 'PowerPoint'){Set-TestSaved $created $true}
    $api.CloseOther($independentId)
    $batchCancelled=Wait-State {param($s)$s.views.Count -eq 3 -and -not $s.diagnostics.closePending}
    $hook.Armed=$false
    if($HostName -eq 'PowerPoint'){Set-TestSaved $created $false}
    if($created.Saved){throw 'Cancelled batch close changed unsaved content.'}
    # This is our own new test document, never a user file.
    Set-TestSaved $extra $true
    if(-not $extra.Saved){throw ('Test Saved flag failed: Extra='+$extra.Name+' Created='+$created.Name+' Host='+$HostName+' Type='+$extra.GetType().FullName)}
    if(-not $api.Close($independentId)){throw 'Test document close failed.'}
    $closedExtra=Wait-State {param($s)$s.views.Count -eq 2 -and -not $s.diagnostics.closePending}
    $api.Activate($newId)
    $newActivated=Wait-State {param($s)$s.activeId -eq $newId}
    for($index=0;$index -lt 2;$index++){
        $api.NewDocument()
        $expectedCount=3+$index
        $batchAdded=Wait-State {param($s)$s.views.Count -eq $expectedCount -and $s.activeId -ne $newId}
        $batchDocument=switch($HostName){'Word'{$app.ActiveDocument};'Excel'{$app.ActiveWorkbook};'PowerPoint'{$app.ActivePresentation}}
        Set-TestSaved $batchDocument $true
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($batchDocument)
    }
    $api.CloseOther($newId)
    $batchCompleted=Wait-State {param($s)$s.views.Count -eq 2 -and -not $s.diagnostics.closePending}
    # A command-driven drop requires visible roots. Restoration failure still
    # fails the test; iconic frame coordinates cannot be used as a drop target.
    foreach($testRoot in @($baseRoot,$newRoot)){if([TestWindows]::IsIconic([IntPtr][long]$testRoot)){[void][TestWindows]::ShowWindowAsync([IntPtr][long]$testRoot,9)}}
    $api.Activate($baseId)
    [TestWindows]::Front([long]$baseRoot)
    $readyForDrop=Wait-State {param($s)$s.views.Count -eq 2 -and $s.activeId -eq $baseId -and -not [TestWindows]::IsIconic([IntPtr][long]$baseRoot) -and -not [TestWindows]::IsIconic([IntPtr][long]$newRoot)}
    $targetPane=$readyForDrop.paneMetrics|Where-Object viewId -eq $baseId
    $dropX=[int]($targetPane.displayLeft+$targetPane.width/2)
    $dropY=[int]($targetPane.displayTop+$targetPane.height/2)
    # Arrange only this owned fixture above unrelated desktop windows. This
    # tests the drop command, without depending on Windows foreground grants.
    [TestWindows]::ExposeDropTarget([long]$baseRoot,$true)
    try{
        $dropDeadline=[DateTime]::UtcNow.AddSeconds(3)
        while([long][TestWindows]::RootAt($dropX,$dropY) -ne [long]$baseRoot -and [DateTime]::UtcNow -lt $dropDeadline){Start-Sleep -Milliseconds 80}
        if([long][TestWindows]::RootAt($dropX,$dropY) -ne [long]$baseRoot){throw 'Target bar is not exposed under the test drop coordinates.'}
        if(-not $api.DropTab($newId,$dropX,$dropY)){throw 'Cross-window drop command failed.'}
    }finally{[TestWindows]::ExposeDropTarget([long]$baseRoot,$false)}
    $merged=Wait-State {param($s)($s.views|Where-Object id -eq $newId).groupId -eq ($s.views|Where-Object id -eq $baseId).groupId}
    if(($merged.views|Where-Object id -eq $newId).hwnd -ne $newRoot -or $created.Saved){throw 'Merge replaced or saved the document.'}
    $api.ToggleVisible()
    if(-not $api.Recover()){throw 'Recovery failed.'}
    foreach($viewId in @($merged.views.id)){
        $api.Activate($viewId)
        $recovered=Wait-State {param($s)$p=$s.paneMetrics|Where-Object viewId -eq $viewId;$s.visible -and $p.displayVisible -and $p.matchesReservedRect}
    }
    $redacted=$api.GetDiagnostics()
    if($redacted -match 'PRIVATE_TEST_MARKER|word-demo|excel-demo|powerpoint-demo|[A-Z]:\\|"title"|"path"|"hwnd"'){throw 'Diagnostic export contains private details.'}
    if(-not $api.SetIndependent($newId,$true)){throw 'Detach failed.'}
    if(-not $api.Close($baseId)){throw 'Base fixture close failed.'}
    $remaining=Wait-State {param($s)$s.views.Count -eq 1 -and $s.views[0].independent}
    if(-not [TestWindows]::IsWindow([IntPtr]$newRoot)){throw 'Closing last grouped tab closed independent document.'}
    $api.Activate($newId)
    if(-not $api.JoinTabs()){throw 'Rejoin from empty group failed.'}
    $api.SetTabWidth($originalWidth,$originalMode)|Out-Null
    $hook.Armed=$true
    if($HostName -eq 'PowerPoint'){Set-TestSaved $created $true}
    $closeRequest=$api.Close($newId)
    if($HostName -ne 'PowerPoint' -and $closeRequest){throw 'Last unsaved document cancellation was ignored.'}
    $lastCancelled=Wait-State {param($s)$s.views.Count -eq 1 -and ($HostName -eq 'PowerPoint' -or -not $s.views[0].saved) -and -not $s.diagnostics.closePending}
    $hook.Armed=$false
    if($HostName -eq 'PowerPoint'){Set-TestSaved $created $false}
    $hook.Dispose();$hook=$null
    Set-TestSaved $created $true # Discard only our deliberately generated marker.
    try{$api.Close($newId)|Out-Null}catch{
        [uint32]$ownerPid=0
        [void][TestWindows]::GetWindowThreadProcessId([IntPtr]$newRoot,[ref]$ownerPid)
        if($ownerPid -eq $officePid -and [TestWindows]::IsWindowVisible([IntPtr]$newRoot)){throw}
    }
    foreach($value in @($created,$extra,$fixture,$collection,$api,$app)){if($value -and [Runtime.InteropServices.Marshal]::IsComObject($value)){try{[void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($value)}catch{}}}
    $api=$null;$app=$null;$fixture=$null;$collection=$null;$created=$null;$extra=$null
    [GC]::Collect();[GC]::WaitForPendingFinalizers()
    $closeDeadline=[DateTime]::UtcNow.AddSeconds(8)
    do{Start-Sleep -Milliseconds 60;$alive=Get-Process -Id $officePid -ErrorAction SilentlyContinue}while($alive -and [DateTime]::UtcNow -lt $closeDeadline)
    if($alive -and -not $DeferProcessExitCheck){throw 'Office process remains after last document close.'}
    $result=[pscustomobject]@{Host=$HostName;Version=$ExpectedVersion;Passed=(-not $alive);LastDocumentExitsApplication=(-not $alive);ProcessId=$officePid;ProcessStartUtc=$officeStartUtc;ProcessExitDeferred=([bool]$alive);NativeCloseCancelPreserved=$true;UnsavedBeforeCloseCancelVerified=($HostName -ne 'PowerPoint');IndependentDocumentSurvives=$true;WidthBoundsPassed=$true;MultipleGroupsPassed=$true;CrossWindowDropPassed=$true;NativeDocumentAndWindowPreserved=$true;BatchScopeAndCancelPassed=$true;RecoverLayoutPassed=$true;DiagnosticsRedacted=$true}
    $result|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $taskRoot ('artifacts\lifecycle-'+$HostName+'.json')) -Encoding UTF8
    $result|Format-List
}catch{
    Write-Output $_.ScriptStackTrace
    throw
}finally{
    if($api){try{$api.SetTabWidth($originalWidth,$originalMode)|Out-Null}catch{}}
    if($hook){try{$hook.Dispose()}catch{}}
}
