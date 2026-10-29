#include "model.h"
#include "window-layout.h"

namespace {
constexpr UINT FRAME_CLOSE=WM_APP+112;
bool owned(HWND h){DWORD pid=0;return h&&IsWindow(h)&&GetWindowThreadProcessId(h,&pid)&&pid==GetCurrentProcessId();}
struct Guard {bool& flag;bool previous;explicit Guard(bool& f):flag(f),previous(f){flag=true;}~Guard(){flag=previous;}};
struct CloseDialog {size_t count;int language;bool remember=false;};
INT_PTR CALLBACK closeDialog(HWND dialog,UINT message,WPARAM wp,LPARAM lp){
    auto data=(CloseDialog*)GetWindowLongPtrW(dialog,DWLP_USER);
    if(message==WM_INITDIALOG){
        data=(CloseDialog*)lp;SetWindowLongPtrW(dialog,DWLP_USER,lp);SetWindowTextW(dialog,i18n::text(data->language,L"关闭文档窗口"));
        int scale=dpi(dialog);auto px=[&](int n){return MulDiv(n,scale,96);};RECT r{0,0,px(450),px(196)};
        AdjustWindowRectEx(&r,(DWORD)GetWindowLongPtrW(dialog,GWL_STYLE),FALSE,(DWORD)GetWindowLongPtrW(dialog,GWL_EXSTYLE));
        SetWindowPos(dialog,nullptr,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER);
        auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int w,int h){auto child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),dialog,(HMENU)(INT_PTR)id,module,nullptr);SendMessageW(child,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);};
        auto text=i18n::closeSummary(data->language,data->count);
        control(L"STATIC",text.c_str(),0,0,18,18,414,24);
        control(L"STATIC",i18n::text(data->language,L"关闭当前文档，还是关闭本组全部文档？\n未保存的更改仍由 Office 询问；取消保存会停止后续关闭。"),0,0,18,49,414,48);
        control(L"BUTTON",i18n::text(data->language,L"记住我的选择（可在标签设置中更改）"),BS_AUTOCHECKBOX|WS_TABSTOP,101,18,108,414,24);
        control(L"BUTTON",i18n::text(data->language,L"关闭当前"),WS_TABSTOP,201,18,153,112,28);
        control(L"BUTTON",i18n::text(data->language,L"关闭本组全部"),WS_TABSTOP,202,142,153,154,28);
        control(L"BUTTON",i18n::text(data->language,L"取消"),WS_TABSTOP|BS_DEFPUSHBUTTON,IDCANCEL,308,153,124,28);
        SetFocus(GetDlgItem(dialog,IDCANCEL));return FALSE;
    }
    if(message==WM_COMMAND&&data){auto cmd=LOWORD(wp);if(cmd==201||cmd==202){data->remember=SendDlgItemMessageW(dialog,101,BM_GETCHECK,0,0)==BST_CHECKED;EndDialog(dialog,cmd-200);return TRUE;}}
    if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(wp)==IDCANCEL)){EndDialog(dialog,0);return TRUE;}return FALSE;
}
FrameCloseChoice prompt(HWND owner,size_t count,int language){
    CloseDialog choice{count,language};struct Template {DLGTEMPLATE dialog;WORD menu,cls,title;} data{};
    data.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;data.dialog.cx=280;data.dialog.cy=135;
    auto result=DialogBoxIndirectParamW(module,&data.dialog,owner,closeDialog,(LPARAM)&choice);
    return {(result==1||result==2)?(int)result:0,choice.remember};
}
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

class RootObserver:public std::enable_shared_from_this<RootObserver> {
    Session* session;HWND window=nullptr;
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR,DWORD_PTR ref){
        auto self=((RootObserver*)ref)->shared_from_this();auto s=self->session;auto keep=s->shared_from_this();
        if(m==WM_NCDESTROY){RemoveWindowSubclass(w,proc,112);self->window=nullptr;if(s->maskSource==w)s->restoreWindowMasks();if(s->sizingRoot==w)s->sizingRoot=nullptr;if(s->queuedLayoutRoot==w){s->queuedLayoutRoot=nullptr;s->queuedLayoutCommand=false;}return DefSubclassProc(w,m,a,b);}
        if((m==WM_CLOSE||(m==WM_SYSCOMMAND&&(a&0xfff0)==SC_CLOSE))&&s->queueFrameClose(w))return 0;
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
void Session::detachWindowObservers(){restoreWindowMasks();if(timer)KillTimer(timer,4);rootObservers.clear();groupSlots.clear();pendingFrameClose=nullptr;operatingRoot=nullptr;sizingRoot=nullptr;queuedLayoutRoot=nullptr;queuedLayoutCommand=false;}
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
}
void Session::requestWindowLayout(HWND root,bool commanded,bool minimize){
    if(stopped||coordinatingWindows||refreshing||executing||!settings.group||!owned(root))return;
    std::shared_ptr<View> source;for(auto& v:views)if(v->hwnd==root)source=v;
    if(!source||source->independent||!showTabs(root))return;
    if(!commanded&&!IsIconic(root)&&GetAncestor(GetForegroundWindow(),GA_ROOT)!=root&&activeTabId(root)!=source->id)return;
    if(queuedLayoutRoot&&queuedLayoutRoot!=root&&queuedLayoutCommand&&!commanded)return;
    bool keepMinimize=queuedLayoutRoot==root&&queuedLayoutMinimize&&!commanded;
    queuedLayoutCommand=commanded||(queuedLayoutRoot==root&&queuedLayoutCommand);queuedLayoutRoot=root;queuedLayoutMinimize=minimize||keepMinimize||IsIconic(root)!=FALSE;
    if(timer)SetTimer(timer,4,40,nullptr);
}
void Session::processWindowLayout(){
    if(timer)KillTimer(timer,4);
    if(stopped){restoreWindowMasks();return;}
    if(excelEditing()){if(queuedLayoutRoot&&timer)SetTimer(timer,4,150,nullptr);return;}
    // Recover an interrupted move/size transaction whose exit notification was
    // lost. A real ongoing native drag retains its frame call/capture.
    if(sizingRoot&&!nativeFrameDepth&&!GetCapture()&&maskActivity&&GetTickCount64()-maskActivity>3000)sizingRoot=nullptr;
    if(!queuedLayoutRoot){if(!windowFrameBusy())restoreWindowMasks();return;}
    if(windowFrameBusy()||refreshing||executing||coordinatingWindows){if(timer)SetTimer(timer,4,40,nullptr);return;}
    HWND root=queuedLayoutRoot,previous=operatingRoot;bool commanded=queuedLayoutCommand,minimize=queuedLayoutMinimize;
    queuedLayoutRoot=nullptr;queuedLayoutCommand=false;queuedLayoutMinimize=false;
    if(commanded)operatingRoot=root;
    try{
        if(minimize&&owned(root)&&!IsIconic(root)){Guard guard(coordinatingWindows);ShowWindow(root,SW_SHOWMINNOACTIVE);}
        rootLayoutChanged(root);
        // Restore only after alignment, while siblings are behind the source.
        if(maskSource==root){Guard guard(coordinatingWindows);for(auto& entry:windowMasks)if(owned(entry.first)&&owned(root)&&!IsIconic(root))SetWindowPos(entry.first,root,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);restoreWindowMasks();}
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
        if(next.minimized){if(!IsIconic(target->hwnd))ShowWindow(target->hwnd,SW_SHOWMINNOACTIVE);}
        else if(!windowlayout::sameSlot(target->hwnd,next.placement,next.bounds)){
            if(!target->moved)windowlayout::capture(target->hwnd,target->original,target->originalBounds);
            if(windowlayout::apply(target->hwnd,next.placement,next.bounds)){target->moved=true;windowlayout::capture(target->hwnd,target->applied,target->appliedBounds);}
        }
    }
    if(!next.minimized&&(foreground||commanded)){
        // Excel may change its active SDI frame while an inactive sibling is
        // restored. Keep the initiating frame active within this UI thread,
        // without taking foreground focus from another application.
        SetActiveWindow(root);
        if(foreground){SetWindowPos(root,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);SetForegroundWindow(root);}
    }
    // Background groups still receive geometry updates, but cannot change the
    // native/COM selection and redirect a later SaveAll to another document.
    if(foreground||commanded)reconcileWindowSelection(source,next.minimized);
}
void Session::reconcileWindowSelection(const std::shared_ptr<View>& source,bool minimized){
    if(!minimized){
        SetActiveWindow(source->hwnd);
        // Word needs the same native/COM agreement as PowerPoint. Run only
        // at idle and after Ribbon transfer, including an unchanged restore slot.
        if((host==Host::Word||host==Host::PowerPoint)&&source->window&&!windowlayout::ribbonInitializing(source->hwnd)){
            auto selected=prop(app.get(),L"ActiveWindow").disp();
            if((selected||host==Host::PowerPoint)&&selected.as<IUnknown>().get()!=source->window.as<IUnknown>().get())call(source->window.get(),L"Activate",DISPATCH_METHOD);
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
    if(!choice.action){Guard guard(frameDialogActive);choice=frameClosePrompt?frameClosePrompt(root,members.size()):prompt(root,members.size(),language());}
    if(choice.action!=1&&choice.action!=2)return;
    if(choice.remember)setFrameCloseAction(choice.action);
    std::vector<unsigned long long> ids;if(choice.action==2)for(auto& v:members)if(v->id!=selected->id)ids.push_back(v->id);ids.push_back(selected->id);
    batchClose(std::move(ids),true);
}
