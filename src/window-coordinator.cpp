#include "model.h"
#include "window-layout.h"
#include "frame-close-dialog.h"
#include <windowsx.h>

namespace {
constexpr UINT FRAME_CLOSE=WM_APP+112;
bool owned(HWND h){DWORD pid=0;return h&&IsWindow(h)&&GetWindowThreadProcessId(h,&pid)&&pid==GetCurrentProcessId();}
struct Guard {bool& flag;bool previous;explicit Guard(bool& f):flag(f),previous(f){flag=true;}~Guard(){flag=previous;}};
}

// A temporary compositor-only veil. WS_VISIBLE and Office's Window.Visible
// remain unchanged, so a masked workbook cannot disappear from enumeration.
// The property is a lifetime token: never restore attributes on a reused HWND.
class WindowMask {
    HWND window=nullptr;bool layered=false;
    COLORREF color=0;BYTE alpha=255;DWORD flags=0;
    static constexpr const wchar_t* token=L"openOfficetab.WindowMask";
public:
    ~WindowMask(){restore();}
    bool conceal(HWND w){
        if(window||!owned(w)||GetWindowThreadProcessId(w,nullptr)!=GetCurrentThreadId()||GetPropW(w,token))return false;
        auto style=GetWindowLongPtrW(w,GWL_EXSTYLE);layered=(style&WS_EX_LAYERED)!=0;
        // UpdateLayeredWindow-based surfaces cannot be snapshotted safely.
        if(layered&&!GetLayeredWindowAttributes(w,&color,&alpha,&flags))return false;
        if(!SetPropW(w,token,(HANDLE)this))return false;window=w;
        if(!layered){SetLastError(0);if(!SetWindowLongPtrW(w,GWL_EXSTYLE,style|WS_EX_LAYERED)&&GetLastError()){restore();return false;}}
        if(!SetLayeredWindowAttributes(w,0,0,LWA_ALPHA)){restore();return false;}
        return true;
    }
    void restore(){
        auto w=window;window=nullptr;if(!owned(w)||GetPropW(w,token)!=(HANDLE)this)return;
        auto style=GetWindowLongPtrW(w,GWL_EXSTYLE);
        if(style&WS_EX_LAYERED){
            COLORREF nowColor=0;BYTE nowAlpha=255;DWORD nowFlags=0;
            // Preserve another component's later appearance changes.
            if(GetLayeredWindowAttributes(w,&nowColor,&nowAlpha,&nowFlags)&&nowAlpha==0&&nowFlags==LWA_ALPHA){
                if(layered)SetLayeredWindowAttributes(w,color,alpha,flags);
                else SetWindowLongPtrW(w,GWL_EXSTYLE,style&~(LONG_PTR)WS_EX_LAYERED);
            }else if(!layered&&!GetLayeredWindowAttributes(w,&nowColor,&nowAlpha,&nowFlags)){
                // conceal() can fail after we add the style, before alpha is set.
                SetWindowLongPtrW(w,GWL_EXSTYLE,style&~(LONG_PTR)WS_EX_LAYERED);
            }
        }
        RemovePropW(w,token);
    }
};

// Observe only this Office STA. No injected helper, global hook, COM calls or
// activation APIs: mouse coordinates come only from a managed document input.
class WindowInputObserver {
    Session* session;HHOOK mouse=nullptr,cbt=nullptr;
    long long inputId=0;ULONGLONG inputTick=0;bool held=false,recording=false;
    inline static thread_local WindowInputObserver* current=nullptr;
    HWND root(HWND w)const{
        HWND r=w?GetAncestor(w,GA_ROOT):nullptr;
        return r&&session->rootObservers.count(r)&&owned(r)?r:nullptr;
    }
    int windowClass(HWND w)const{
        wchar_t name[80]{};GetClassNameW(w,name,80);
        if(!_wcsicmp(name,L"NetUIHWND"))return 1;if(!_wcsicmp(name,L"openOfficetab.Tabs"))return 2;
        if(!_wcsicmp(name,L"openOfficetab.Seam"))return 3;return 0;
    }
    void windows(HWND source,int phase,UINT message){
        size_t count=0;unsigned group=session->groupForWindow(source);
        windowlayout::PhysicalCoordinates coordinates;
        for(auto& view:session->views){
            if(view->groupId!=group||!root(view->hwnd))continue;
            if(count++>=8)break;
            RECT bounds{};GetWindowRect(view->hwnd,&bounds);COLORREF color=0;BYTE alpha=255;DWORD alphaFlags=0;
            bool alphaReadable=GetLayeredWindowAttributes(view->hwnd,&color,&alpha,&alphaFlags)!=FALSE;
            // Relative rank among managed roots; outside HWNDs are never emitted.
            int rank=0;HWND above=GetWindow(view->hwnd,GW_HWNDPREV);unsigned scanned=0;
            for(;above&&scanned<512;above=GetWindow(above,GW_HWNDPREV),++scanned)if(root(above))++rank;
            logEvent(L"window.input.view",{{"inputId",recent()},{"phase",phase},{"message",message},{"sourceHwnd",diagnostic::window(source)},{"hwnd",diagnostic::window(view->hwnd)},{"viewId",(long long)view->id},{"x",bounds.left},{"y",bounds.top},{"width",bounds.right-bounds.left},{"height",bounds.bottom-bounds.top},{"dpi",dpi(view->hwnd)},{"zRank",rank},{"rankTruncated",above!=nullptr},{"extendedStyle",(long long)(ULONG_PTR)GetWindowLongPtrW(view->hwnd,GWL_EXSTYLE)},{"masked",GetPropW(view->hwnd,L"openOfficetab.WindowMask")!=nullptr},{"alphaReadable",alphaReadable},{"alpha",alpha},{"alphaFlags",alphaFlags},{"truncated",session->views.size()>8}});
        }
    }
    static LRESULT CALLBACK mouseProc(int code,WPARAM wp,LPARAM lp){
        auto self=current;
        if(code==HC_ACTION&&self&&!self->recording&&!self->session->stopped){
            bool down=wp==WM_LBUTTONDOWN||wp==WM_NCLBUTTONDOWN||wp==WM_LBUTTONDBLCLK||wp==WM_NCLBUTTONDBLCLK;
            bool up=wp==WM_LBUTTONUP||wp==WM_NCLBUTTONUP;
            if(down||up){
                auto data=(MOUSEHOOKSTRUCT*)lp;HWND r=self->root(data->hwnd);
                if(r){
                    Guard guard(self->recording);diagnostic::Scope cause(diagnostic::WindowMessage);
                    if(down){self->inputId=runtimelog::traceContext.id;self->held=true;}
                    self->inputTick=GetTickCount64();
                    POINT point=data->pt;HWND hit=self->root(WindowFromPoint(point));
                    logEvent(L"window.input",{{"inputId",self->recent()},{"message",(long long)wp},{"receiverHwnd",diagnostic::window(data->hwnd)},{"hwnd",diagnostic::window(r)},{"hitHwnd",diagnostic::window(hit)},{"hitCode",(long long)(int)data->wHitTestCode},{"receiverClass",self->windowClass(data->hwnd)},{"x",point.x},{"y",point.y},{"foregroundHwnd",diagnostic::window(self->root(GetForegroundWindow()))},{"activeHwnd",diagnostic::window(self->root(GetActiveWindow()))}});
                    self->windows(r,0,(UINT)wp);if(up)self->held=false;
                }else{self->held=false;self->inputId=0;}
            }
        }
        return CallNextHookEx(nullptr,code,wp,lp);
    }
    static LRESULT CALLBACK cbtProc(int code,WPARAM wp,LPARAM lp){
        auto self=current;
        if(code==HCBT_ACTIVATE&&self&&!self->recording&&!self->session->stopped){
            HWND r=self->root((HWND)wp);if(r){Guard guard(self->recording);diagnostic::Scope cause(diagnostic::WindowMessage);
                auto data=(CBTACTIVATESTRUCT*)lp;
                logEvent(L"window.activation.request",{{"inputId",self->recent()},{"hwnd",diagnostic::window(r)},{"mouseActivation",data->fMouse!=FALSE},{"previousHwnd",diagnostic::window(self->root(data->hWndActive))},{"foregroundHwnd",diagnostic::window(self->root(GetForegroundWindow()))},{"activeHwnd",diagnostic::window(self->root(GetActiveWindow()))}});
                self->windows(r,1,WM_ACTIVATE);
            }
        }
        return CallNextHookEx(nullptr,code,wp,lp);
    }
public:
    explicit WindowInputObserver(Session* s):session(s){}
    ~WindowInputObserver(){if(mouse)UnhookWindowsHookEx(mouse);if(cbt)UnhookWindowsHookEx(cbt);if(current==this)current=nullptr;}
    void attach(){
        if(current&&current!=this){logEvent(L"window.input.unavailable",{{"reason",1}});return;}
        current=this;mouse=SetWindowsHookExW(WH_MOUSE,mouseProc,nullptr,GetCurrentThreadId());
        DWORD mouseError=mouse?0:GetLastError();cbt=SetWindowsHookExW(WH_CBT,cbtProc,nullptr,GetCurrentThreadId());DWORD cbtError=cbt?0:GetLastError();
        logEvent(L"window.input.observer",{{"mouseHook",mouse!=nullptr},{"activationHook",cbt!=nullptr},{"mouseError",mouseError},{"activationError",cbtError}});
    }
    bool available()const{return mouse&&cbt;}
    long long recent()const{return inputId&&(held||GetTickCount64()-inputTick<=3000)?inputId:0;}
    void message(HWND w,UINT m,WPARAM wp,LPARAM lp,int phase,LRESULT result=0){
        if(recording||session->stopped||!root(w))return;Guard guard(recording);
        logEvent(L"window.input.message",{{"inputId",recent()},{"hwnd",diagnostic::window(w)},{"phase",phase},{"message",m},{"activation",m==WM_ACTIVATE?LOWORD(wp):0},{"hitCode",m==WM_MOUSEACTIVATE?(short)LOWORD(lp):0},{"mouseMessage",m==WM_MOUSEACTIVATE?HIWORD(lp):0},{"result",phase==2?(long long)result:0},{"foregroundHwnd",diagnostic::window(root(GetForegroundWindow()))},{"activeHwnd",diagnostic::window(root(GetActiveWindow()))},{"activeId",(long long)session->activeId}});
        if(m==WM_ACTIVATE||m==WM_MOUSEACTIVATE)windows(w,phase,m);
    }
};
bool Session::inputTraceAvailable()const{return inputObserver&&inputObserver->available();}

class RootObserver:public std::enable_shared_from_this<RootObserver> {
    Session* session;HWND window=nullptr;
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR,DWORD_PTR ref){
        diagnostic::Scope cause(diagnostic::WindowMessage,0,false);
        auto self=((RootObserver*)ref)->shared_from_this();auto s=self->session;auto keep=s->shared_from_this();
        if(m==WM_NCDESTROY){RemoveWindowSubclass(w,proc,112);self->window=nullptr;if(s->maskSource==w)s->restoreWindowMasks();if(s->sizingRoot==w)s->sizingRoot=nullptr;if(s->queuedLayoutRoot==w){s->cancelLayoutDiagnostic(4);s->queuedLayoutRoot=nullptr;s->queuedLayoutCommand=false;}return DefSubclassProc(w,m,a,b);}
        if((m==WM_CLOSE||(m==WM_SYSCOMMAND&&(a&0xfff0)==SC_CLOSE))&&s->queueFrameClose(w))return 0;
        auto input=s->inputObserver;
        bool inputMessage=m==WM_ACTIVATE||m==WM_MOUSEACTIVATE||m==WM_NCLBUTTONDOWN||m==WM_NCLBUTTONUP||m==WM_NCLBUTTONDBLCLK;
        if(input&&inputMessage)input->message(w,m,a,b,1);
        auto command=a&0xfff0;
        bool frameOperation=m==WM_SYSCOMMAND&&(command==SC_MOVE||command==SC_SIZE||command==SC_MINIMIZE||command==SC_MAXIMIZE||command==SC_RESTORE);
        bool geometryChanging=m==WM_WINDOWPOSCHANGING&&b&&(((WINDOWPOS*)b)->flags&(SWP_NOMOVE|SWP_NOSIZE))!=(SWP_NOMOVE|SWP_NOSIZE);
        // Veil siblings before Office/DefWindowProc can move the active frame.
        if(frameOperation||m==WM_ENTERSIZEMOVE||geometryChanging)s->beginWindowTransition(w);
        if((m==WM_ENABLE&&!a)||m==WM_CANCELMODE){
            if(s->sizingRoot==w)s->sizingRoot=nullptr;
            if(s->maskSource==w)s->requestWindowLayout(w,true);
            if(m==WM_CANCELMODE&&!s->nativeFrameDepth&&s->operatingRoot==w)s->operatingRoot=nullptr;
        }
        HWND previous=s->operatingRoot;if(frameOperation||m==WM_ENTERSIZEMOVE)s->operatingRoot=w;
        if(m==WM_ENTERSIZEMOVE){s->sizingRoot=w;RECT bounds{};GetWindowRect(w,&bounds);logEvent(L"window.drag.begin",{{"hwnd",(long long)(UINT_PTR)w},{"viewId",(long long)s->displayedTabId(w)},{"activeId",(long long)s->activeId},{"x",bounds.left},{"y",bounds.top},{"width",bounds.right-bounds.left},{"height",bounds.bottom-bounds.top}});}
        if(frameOperation){logEvent(L"window.command",{{"hwnd",(long long)(UINT_PTR)w},{"command",(long long)command},{"activeId",(long long)s->activeId}});++s->nativeFrameDepth;}
        auto result=DefSubclassProc(w,m,a,b);
        if(input&&inputMessage)input->message(w,m,a,b,2,result);
        if(frameOperation)--s->nativeFrameDepth;
        if(m==WM_EXITSIZEMOVE){RECT bounds{};GetWindowRect(w,&bounds);logEvent(L"window.drag.end",{{"hwnd",(long long)(UINT_PTR)w},{"viewId",(long long)s->displayedTabId(w)},{"activeId",(long long)s->activeId},{"x",bounds.left},{"y",bounds.top},{"width",bounds.right-bounds.left},{"height",bounds.bottom-bounds.top}});if(s->sizingRoot==w)s->sizingRoot=nullptr;}
        // Never resize siblings or call the Office/shell COM interfaces from
        // an Office frame's size callback. Its layout is still in progress.
        if(frameOperation||m==WM_WINDOWPOSCHANGED||m==WM_SIZE||m==WM_EXITSIZEMOVE){
            if(s->maskSource==w)s->maskActivity=GetTickCount64();
            // Office's custom caption need not deliver SC_MINIMIZE to this
            // root. Preserve SIZE_MINIMIZED before Office activates a sibling.
            bool minimized=m==WM_SIZE&&a==SIZE_MINIMIZED;
            s->requestWindowLayout(w,frameOperation||s->operatingRoot==w||minimized,minimized||(frameOperation&&command==SC_MINIMIZE));
        }
        if(frameOperation)s->operatingRoot=previous;else if(m==WM_EXITSIZEMOVE&&s->operatingRoot==w)s->operatingRoot=nullptr;
        // Taskbar/Alt-Tab activation need not change geometry or raise an
        // Office event. Queue reconciliation without COM work in this callback.
        if(m==WM_ACTIVATE){logEvent(L"window.activate",{{"hwnd",(long long)(UINT_PTR)w},{"activation",LOWORD(a)},{"viewId",(long long)s->displayedTabId(w)},{"activeId",(long long)s->activeId},{"frameBusy",s->windowFrameBusy()}});if(LOWORD(a)!=WA_INACTIVE){s->redraw();s->defer();}}
        if(m==WM_THEMECHANGED||m==WM_SETTINGCHANGE){s->pollTheme(true);s->redraw();}
        return result;
    }
public:
    explicit RootObserver(Session* s):session(s){}
    ~RootObserver(){if(window&&IsWindow(window))RemoveWindowSubclass(window,proc,112);}
    bool attach(HWND w){if(!owned(w)||GetWindowThreadProcessId(w,nullptr)!=GetCurrentThreadId())return false;if(!SetWindowSubclass(w,proc,112,(DWORD_PTR)this))return false;window=w;return true;}
};

void Session::restoreWindowMasks(){Guard guard(coordinatingWindows);windowMasks.clear();maskSource=nullptr;maskActivity=0;}
void Session::beginWindowTransition(HWND root){
    if(stopped||coordinatingWindows||executing||refreshing||!settings.group||!owned(root)||!IsWindowEnabled(root)||!showTabs(root))return;
    if(excelEditing())return;
    if(maskSource==root)return;
    auto source=std::find_if(views.begin(),views.end(),[&](auto& v){return v->hwnd==root&&!v->independent;});if(source==views.end())return;
    if(activeTabId(root)!=(*source)->id&&operatingRoot!=root&&GetAncestor(GetForegroundWindow(),GA_ROOT)!=root)return;
    restoreWindowMasks();Guard guard(coordinatingWindows);maskSource=root;maskActivity=GetTickCount64();
    for(auto& view:views)if(view->hwnd!=root&&!view->independent&&view->groupId==(*source)->groupId&&owned(view->hwnd)&&IsWindowEnabled(view->hwnd)&&IsWindowVisible(view->hwnd)){
        auto mask=std::make_shared<WindowMask>();if(mask->conceal(view->hwnd))windowMasks.emplace(view->hwnd,std::move(mask));else log(L"window.mask.unavailable");
    }
}
void Session::detachWindowObservers(){inputObserver.reset();cancelLayoutDiagnostic(3);restoreWindowMasks();if(timer)KillTimer(timer,4);rootObservers.clear();groupSlots.clear();pendingFrameClose=nullptr;operatingRoot=nullptr;sizingRoot=nullptr;queuedLayoutRoot=nullptr;queuedLayoutCommand=false;}
void Session::syncWindowObservers(){
    if(stopped)return;
    std::set<HWND> live;for(auto& v:views)if(owned(v->hwnd))live.insert(v->hwnd);
    for(auto it=rootObservers.begin();it!=rootObservers.end();)if(!live.count(it->first))it=rootObservers.erase(it);else ++it;
    Guard guard(coordinatingWindows);
    // Capture one shared slot per group before attaching newly-created roots.
    // A new document inherits this slot rather than its host's default size.
    for(auto& v:views)if(settings.group&&!v->independent&&owned(v->hwnd)&&v->id==activeTabId(v->hwnd)&&!groupSlots.count(v->groupId)){
        GroupSlot slot;if(windowlayout::capture(v->hwnd,slot.placement,slot.bounds)){slot.minimized=IsIconic(v->hwnd)!=FALSE;groupSlots[v->groupId]=slot;}
    }
    for(auto& v:views)if(owned(v->hwnd)&&!rootObservers.count(v->hwnd)){
        if(windowlayout::ribbonInitializing(v->hwnd))continue;
        auto observer=std::make_shared<RootObserver>(this);if(observer->attach(v->hwnd))rootObservers[v->hwnd]=std::move(observer);
        auto it=groupSlots.find(v->groupId);
        if(settings.group&&!v->independent&&it!=groupSlots.end()&&!it->second.minimized&&!windowlayout::sameSlot(v->hwnd,it->second.placement,it->second.bounds)){
            if(!v->moved)windowlayout::capture(v->hwnd,v->original,v->originalBounds);
            if(windowlayout::apply(v->hwnd,it->second.placement,it->second.bounds)){v->moved=true;windowlayout::capture(v->hwnd,v->applied,v->appliedBounds);}
        }
    }
    if(!settings.group)groupSlots.clear();
    else for(auto it=groupSlots.begin();it!=groupSlots.end();)if(std::none_of(views.begin(),views.end(),[&](auto& v){return !v->independent&&v->groupId==it->first;}))it=groupSlots.erase(it);else ++it;
    if(!rootObservers.empty()&&!inputObserver){inputObserver=std::make_shared<WindowInputObserver>(this);inputObserver->attach();}
}
void Session::requestWindowLayout(HWND root,bool commanded,bool minimize){
    if(stopped||coordinatingWindows||refreshing||executing||!settings.group||!owned(root))return;
    std::shared_ptr<View> source;for(auto& v:views)if(v->hwnd==root)source=v;
    if(!source||source->independent||!showTabs(root))return;
    if(!commanded&&!IsIconic(root)&&GetAncestor(GetForegroundWindow(),GA_ROOT)!=root&&activeTabId(root)!=source->id)return;
    if(queuedLayoutRoot&&queuedLayoutRoot!=root&&queuedLayoutCommand&&!commanded)return;
    bool keepMinimize=queuedLayoutRoot==root&&queuedLayoutMinimize&&!commanded;
    if(queuedLayoutId&&queuedLayoutRoot==root&&((commanded&&!queuedLayoutCommand)||(minimize&&!queuedLayoutMinimize)))logEvent(L"layout.update",{{"layoutId",queuedLayoutId},{"requestTrace",runtimelog::traceContext.id},{"commanded",commanded},{"minimized",minimize}});
    if(!queuedLayoutId||queuedLayoutRoot!=root){cancelLayoutDiagnostic(1);queuedLayoutId=InterlockedIncrement64(&runtimelog::traceCounter);queuedLayoutTrace=runtimelog::traceContext.id;logEvent(L"layout.queue",{{"layoutId",queuedLayoutId},{"requestTrace",queuedLayoutTrace},{"hwnd",diagnostic::window(root)},{"commanded",commanded},{"minimized",minimize},{"activeId",(long long)activeId}});}
    queuedLayoutCommand=commanded||(queuedLayoutRoot==root&&queuedLayoutCommand);queuedLayoutRoot=root;queuedLayoutMinimize=minimize||keepMinimize||IsIconic(root)!=FALSE;
    if(timer)SetTimer(timer,4,40,nullptr);
}
void Session::processWindowLayout(){
    if(timer)KillTimer(timer,4);
    if(stopped){restoreWindowMasks();return;}
    if(excelEditing()){if(queuedLayoutRoot&&timer)SetTimer(timer,4,150,nullptr);if(queuedLayoutRoot&&layoutDiagnosticGate.take(GetTickCount64(),500))logEvent(L"layout.defer",{{"layoutId",queuedLayoutId},{"reason",1},{"suppressed",layoutDiagnosticGate.drain()}});return;}
    // Recover an interrupted move/size transaction whose exit notification was
    // lost. A real ongoing native drag retains its frame call/capture.
    if(sizingRoot&&!nativeFrameDepth&&!GetCapture()&&maskActivity&&GetTickCount64()-maskActivity>3000)sizingRoot=nullptr;
    if(!queuedLayoutRoot){if(!windowFrameBusy())restoreWindowMasks();return;}
    if(windowFrameBusy()||refreshing||executing||coordinatingWindows){if(timer)SetTimer(timer,4,40,nullptr);if(layoutDiagnosticGate.take(GetTickCount64(),500))logEvent(L"layout.defer",{{"layoutId",queuedLayoutId},{"reason",2},{"frameBusy",windowFrameBusy()},{"refreshing",refreshing},{"executing",executing},{"suppressed",layoutDiagnosticGate.drain()}});return;}
    diagnostic::Scope cause(diagnostic::Layout,queuedLayoutTrace,true,queuedLayoutId);
    logEvent(L"layout.run",{{"layoutId",queuedLayoutId},{"hwnd",diagnostic::window(queuedLayoutRoot)},{"commanded",queuedLayoutCommand},{"minimized",queuedLayoutMinimize}});queuedLayoutId=0;queuedLayoutTrace=0;
    HWND root=queuedLayoutRoot,previous=operatingRoot;bool commanded=queuedLayoutCommand,minimize=queuedLayoutMinimize;
    queuedLayoutRoot=nullptr;queuedLayoutCommand=false;queuedLayoutMinimize=false;
    if(commanded)operatingRoot=root;
    try{
        if(minimize&&owned(root)&&!IsIconic(root)){Guard guard(coordinatingWindows);diagnostic::show(root,SW_SHOWMINNOACTIVE);}
        rootLayoutChanged(root);
        // Restore only after alignment, while siblings are behind the source.
        if(maskSource==root){Guard guard(coordinatingWindows);for(auto& entry:windowMasks)if(owned(entry.first)&&owned(root)&&!IsIconic(root))diagnostic::position(entry.first,root,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);restoreWindowMasks();}
        else if(maskSource)restoreWindowMasks();
    }catch(...){restoreWindowMasks();operatingRoot=previous;throw;}
    operatingRoot=previous;
}
void Session::rootLayoutChanged(HWND root){
    if(stopped||executing||refreshing||coordinatingWindows||windowFrameBusy()||!settings.group||!owned(root)||!IsWindowEnabled(root))return;
    if(excelEditing())return;
    std::shared_ptr<View> source;for(auto& v:views)if(v->hwnd==root)source=v;
    if(!source||source->independent||!showTabs(root))return;
    // Office may activate a sibling as soon as the source starts minimizing.
    // A minimize event still belongs to the whole group in that transition.
    bool commanded=operatingRoot==root;
    if(!commanded&&!IsIconic(root)&&GetAncestor(GetForegroundWindow(),GA_ROOT)!=root&&activeTabId(root)!=source->id)return;
    GroupSlot next;if(!windowlayout::capture(root,next.placement,next.bounds))return;next.minimized=IsIconic(root)!=FALSE;
    auto old=groupSlots.find(source->groupId);
    bool peersMatch=std::all_of(views.begin(),views.end(),[&](auto& v){return v==source||v->independent||v->groupId!=source->groupId||!owned(v->hwnd)||!IsWindowEnabled(v->hwnd)||(next.minimized?IsIconic(v->hwnd)!=FALSE:windowlayout::sameSlot(v->hwnd,next.placement,next.bounds));});
    if(peersMatch&&old!=groupSlots.end()&&old->second.minimized==next.minimized&&(next.minimized||(old->second.placement.showCmd==next.placement.showCmd&&EqualRect(&old->second.bounds,&next.bounds)))){
        // An unchanged slot must not post another refresh. In the modal
        // move/size loop this used to feed REFRESH back into itself forever.
        if(commanded){Guard guard(coordinatingWindows);reconcileWindowSelection(source,next.minimized);}return;
    }
    Guard guard(coordinatingWindows);bool foreground=GetAncestor(GetForegroundWindow(),GA_ROOT)==root;
    groupSlots[source->groupId]=next;
    for(auto& target:views)if(target!=source&&!target->independent&&target->groupId==source->groupId&&owned(target->hwnd)&&IsWindowEnabled(target->hwnd)){
        if(next.minimized){if(!IsIconic(target->hwnd))diagnostic::show(target->hwnd,SW_SHOWMINNOACTIVE);}
        else if(!windowlayout::sameSlot(target->hwnd,next.placement,next.bounds)){
            if(!target->moved)windowlayout::capture(target->hwnd,target->original,target->originalBounds);
            if(windowlayout::apply(target->hwnd,next.placement,next.bounds)){target->moved=true;windowlayout::capture(target->hwnd,target->applied,target->appliedBounds);}
        }
    }
    if(!next.minimized&&(foreground||commanded)){
        // Excel may change its active SDI frame while an inactive sibling is
        // restored. Keep the initiating frame active within this UI thread,
        // without taking foreground focus from another application.
        diagnostic::active(root);
        if(foreground){diagnostic::position(root,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);diagnostic::foreground(root);}
    }
    // Background groups still receive geometry updates, but cannot change the
    // native/COM selection and redirect a later SaveAll to another document.
    if(foreground||commanded)reconcileWindowSelection(source,next.minimized);
}
void Session::reconcileWindowSelection(const std::shared_ptr<View>& source,bool minimized){
    if(!minimized){
        diagnostic::active(source->hwnd);
        // Word needs the same native/COM agreement as PowerPoint. Run only
        // at idle and after Ribbon transfer, including an unchanged restore slot.
        if((host==Host::Word||host==Host::PowerPoint)&&source->window&&!windowlayout::ribbonInitializing(source->hwnd)){
            auto selected=prop(app.get(),L"ActiveWindow").disp();
            if((selected||host==Host::PowerPoint)&&selected.as<IUnknown>().get()!=source->window.as<IUnknown>().get())diagnostic::perform(diagnostic::ComActivate,source->hwnd,[&]{call(source->window.get(),L"Activate",DISPATCH_METHOD);return true;});
        }
    }
    setActiveView(source->id);defer();
}
bool Session::queueFrameClose(HWND root){
    if(stopped||executing||refreshing||!settings.group||!timer||!owned(root)||!showTabs(root))return false;
    if(pendingCloseId){
        // PowerPoint can post another WM_CLOSE after the initiating native
        // call returns. It is the completion of our accepted request, not a
        // second user choice; swallowing it leaves the close pending forever.
        auto target=find(pendingCloseId);return !target||target->hwnd!=root;
    }
    if(pendingFrameClose||frameDialogActive)return true;
    pendingFrameClose=root;if(!PostMessageW(timer,FRAME_CLOSE,0,0)){pendingFrameClose=nullptr;return false;}return true;
}
void Session::processFrameClose(){
    HWND root=pendingFrameClose;pendingFrameClose=nullptr;
    if(stopped||executing||refreshing||frameDialogActive||!owned(root))return;
    std::shared_ptr<View> selected;for(auto& v:views)if(v->hwnd==root)selected=v;
    if(!selected||selected->independent)return;
    auto members=tabViews(root);FrameCloseChoice choice{members.size()>1?settings.frameCloseAction:1,false};
    if(!choice.action){Guard guard(frameDialogActive);choice=frameClosePrompt?frameClosePrompt(root,members.size()):framecloseui::prompt(root,members.size(),language(),host,darkMode());}
    if(choice.action!=1&&choice.action!=2)return;
    if(choice.remember)setFrameCloseAction(choice.action);
    std::vector<unsigned long long> ids;if(choice.action==2)for(auto& v:members)if(v->id!=selected->id)ids.push_back(v->id);ids.push_back(selected->id);
    batchClose(std::move(ids),true);
}
