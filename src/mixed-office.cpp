#include "model.h"
#include "mixed-office.h"
#include "window-layout.h"
#include "tab-menu.h"

namespace mixedoffice {
Client::Client(){
    GUID guid{};if(SUCCEEDED(CoCreateGuid(&guid)))memcpy(&owner,&guid,sizeof(owner));
    FILETIME created{},exit{},kernel{},user{};
    if(GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user))start=(uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime;
}
Client::~Client()=default;
uint64_t Client::localKey(const Session& s,uint64_t id)const{
    if(id&remoteBit)return remote(id)?id:0;
    for(auto& m:snapshot)if(m.owner==owner&&m.start==start&&m.view==id&&m.pid==GetCurrentProcessId()){
        for(auto& v:s.views)if(v->id==id&&v->hwnd==window(m))return m.key;
    }
    return 0;
}
bool Client::contains(HWND root)const{for(auto& m:snapshot)if(m.owner==owner&&m.group&&window(m)==root&&live(m))return true;return false;}
std::shared_ptr<View> Client::remote(uint64_t key)const{
    if(!(key&remoteBit))return {};
    for(auto& m:snapshot)if(m.key==key&&m.owner!=owner&&m.group&&live(m)){
        bool shared=false;for(auto& a:snapshot)if(a.owner==owner&&a.group==m.group&&live(a))shared=true;
        if(!shared)return {};
        auto v=std::make_shared<View>();v->id=key;v->hwnd=window(m);v->title.assign(m.title,wcsnlen_s(m.title,256));
        v->saved=m.saved!=0;v->locked=m.locked!=0;v->readOnly=true;v->mixedGroup=m.group;v->tabHost=(int)m.host;v->manualTabWidth=(int)m.width;return v;
    }
    return {};
}
bool Client::members(const Session& s,HWND root,std::vector<std::shared_ptr<View>>& out)const{
    uint64_t group=0;for(auto& m:snapshot)if(m.owner==owner&&window(m)==root&&live(m))group=m.group;
    if(!group)return false;
    std::vector<Member> ordered;for(auto& m:snapshot)if(m.group==group&&live(m))ordered.push_back(m);
    std::sort(ordered.begin(),ordered.end(),[](auto& a,auto& b){return a.order<b.order;});
    for(auto& m:ordered){if(m.owner==owner){for(auto& v:s.views)if(v->id==m.view&&v->hwnd==window(m))out.push_back(v);}else if(auto v=remote(m.key))out.push_back(v);}
    return true;
}
bool Client::request(Session& s,uint64_t id,Command command,HWND root){
    if(s.excelEditing()||s.windowFrameBusy()||s.frameDialogActive)return false;
    root=root?GetAncestor(root,GA_ROOT):GetAncestor(GetForegroundWindow(),GA_ROOT);
    uint64_t source=0;for(auto& m:snapshot)if(m.owner==owner&&window(m)==root)source=m.key;
    bool accepted=false;
    store.access([&](Ledger& state){normalize(state);auto target=find(state,id);if(target&&enqueue(state,source,id,command,GetTickCount64())){accepted=true;AllowSetForegroundWindow(target->pid);}});
    if(accepted){s.lastOperation=L"mixed.request.pending";s.lastError=E_PENDING;}return accepted;
}
bool Client::reorder(Session& s,uint64_t from,uint64_t to){
    if(from==to)return false;auto a=localKey(s,from),b=localKey(s,to);bool done=false;
    store.access([&](Ledger& state){auto first=find(state,a),last=find(state,b);if(!first||!last||!first->group||first->group!=last->group)return;
        bool ours=false;for(auto& m:state.members)if(m.owner==owner&&m.group==first->group)ours=true;if(!ours)return;
        std::vector<Member*> ordered;for(auto& m:state.members)if(m.key&&m.group==first->group)ordered.push_back(&m);
        std::sort(ordered.begin(),ordered.end(),[](auto x,auto y){return x->order<y->order;});
        auto old=std::find(ordered.begin(),ordered.end(),first);ordered.erase(old);auto at=std::find(ordered.begin(),ordered.end(),last);ordered.insert(at,first);
        uint64_t order=1;for(auto m:ordered)m->order=order++;done=true;
    });if(done)tick(s);return done;
}
void Client::tick(Session& s){
    if(ticking||!owner||!start||s.stopped||s.refreshing||s.executing||s.coordinatingWindows)return;
    struct Busy{bool& v;Busy(bool& value):v(value){v=true;}~Busy(){v=false;}} busy(ticking);
    const auto now=GetTickCount64();bool ready=!s.windowFrameBusy()&&!s.frameDialogActive&&!s.excelEditing();
    std::vector<Member> nextSnapshot,commands;
    bool ok=store.access([&](Ledger& state){
        normalize(state);
        for(auto& m:state.members)if(m.owner==owner){bool exists=false;for(auto& v:s.views)if(v->id==m.view&&v->hwnd==window(m))exists=true;
            if(!exists){if(live(m))RemovePropW(window(m),windowProperty);m={};}}
        for(auto& v:s.views){
            v->tabHost=(int)s.host;DWORD pid=0;GetWindowThreadProcessId(v->hwnd,&pid);
            if(!v->hwnd||v->id>=remoteBit||pid!=GetCurrentProcessId()||!IsWindowVisible(v->hwnd)||v->independent||!s.settings.visible)continue;
            Member* current=nullptr;for(auto& m:state.members)if(m.owner==owner&&m.view==v->id&&window(m)==v->hwnd)current=&m;
            if(!current)for(auto& m:state.members)if(!m.key){auto key=next(state);if(!key)break;
                if(!SetPropW(v->hwnd,windowProperty,(HANDLE)(UINT_PTR)(key&0x7fffffff)))break;
                m={};m.key=key;m.owner=owner;m.start=start;m.pid=pid;m.view=v->id;m.window=(uint64_t)(UINT_PTR)v->hwnd;m.width=v->manualTabWidth;current=&m;break;}
            if(!current)continue;
            current->host=(uint32_t)s.host;current->seen=now;current->ready=ready&&IsWindowEnabled(v->hwnd);current->saved=v->saved;current->locked=v->locked;
            wcsncpy_s(current->title,v->title.c_str(),_TRUNCATE);
            if(current->command){commands.push_back(*current);current->command=None;current->request=0;}
        }
        normalize(state);
        HWND foreground=GetAncestor(GetForegroundWindow(),GA_ROOTOWNER);
        for(auto& m:state.members)if(m.group&&window(m)==foreground&&live(m)){
            auto key=m.key,group=m.group;for(auto& n:state.members)if(n.group==group)n.selected=key;break;
        }
        for(auto& m:state.members)if(m.key)nextSnapshot.push_back(m);
    });
    if(!ok&&!store.failed())return;
    if(ok)snapshot=std::move(nextSnapshot);else snapshot.clear();
    // A ledger failure makes mixed mode inert. Local Office tabs stay usable.
    bool changed=false;
    for(auto& v:s.views){uint64_t group=0;for(auto& m:snapshot)if(ok&&m.owner==owner&&m.view==v->id&&window(m)==v->hwnd)group=m.group;
        if(group&&!v->mixedGroup){
            s.restoreWindowMasks();v->mixedOriginalGroup=v->groupId;v->mixedOriginalIndependent=v->independent;
            if(v->taskbarRemoved&&s.taskbar)s.taskbar->AddTab(v->hwnd);v->taskbarRemoved=false;
            windowlayout::capture(v->hwnd,v->original,v->originalBounds);v->moved=false;
            v->groupId=s.nextGroupId++;s.groupActives[v->groupId]=v->id;v->independent=false;changed=true;
        }else if(!group&&v->mixedGroup){s.mixedPresentation(v->hwnd,false);s.restoreView(*v);s.groupActives.erase(v->groupId);v->groupId=v->mixedOriginalGroup;v->independent=v->mixedOriginalIndependent;changed=true;}
        if(group!=v->mixedGroup)changed=true;v->mixedGroup=group;
        for(auto& m:snapshot)if(group&&m.owner==owner&&m.view==v->id&&m.width<=360)v->manualTabWidth=(int)m.width;
    }
    // Only the actual foreground member supplies the slot. Each STA moves
    // its own windows and retains their placement restoration bookkeeping.
    HWND foreground=GetAncestor(GetForegroundWindow(),GA_ROOT);
    const Member* leader=nullptr;for(auto& m:snapshot)if(m.group&&window(m)==foreground&&available(m,now))leader=&m;
    if(ok&&ready&&leader&&!IsIconic(foreground)){
        WINDOWPLACEMENT placement{sizeof(placement)};RECT bounds{};
        if(windowlayout::capture(foreground,placement,bounds)&&!windowlayout::minimized(placement))for(auto& v:s.views)
            if(v->mixedGroup==leader->group&&v->hwnd!=foreground&&IsWindowEnabled(v->hwnd)&&!IsIconic(v->hwnd)&&!windowlayout::sameSlot(v->hwnd,placement,bounds)){
                if(windowlayout::apply(v->hwnd,placement,bounds)){v->moved=true;windowlayout::capture(v->hwnd,v->applied,v->appliedBounds);}
            }
    }
    if(changed){s.syncPanes();s.updateTaskbar();}
    for(auto& v:s.views)if(v->mixedGroup){
        const Member* selected=nullptr;for(auto& m:snapshot)if(m.owner==owner&&m.view==v->id)for(auto& n:snapshot)if(n.key==m.selected&&n.group==m.group)selected=&n;
        // A nonresponsive selected endpoint cannot strand its siblings.
        bool healthy=selected&&live(*selected)&&now>=selected->seen&&now-selected->seen<2000;
        if(!healthy||window(*selected)==v->hwnd||!IsWindowEnabled(v->hwnd))s.mixedPresentation(v->hwnd,false);
        else s.mixedPresentation(v->hwnd,true);
    }
    // Titles, order and modifications can change in another process.
    if(ok&&std::any_of(s.views.begin(),s.views.end(),[](auto& v){return v->mixedGroup!=0;}))s.redraw();
    for(auto& command:commands){
        const Member* source=nullptr;for(auto& m:snapshot)if(m.key==command.requestSource)source=&m;
        if(!ok||!ready||!source||!command.group||source->group!=command.group||!available(*source,GetTickCount64())||
           GetTickCount64()-command.requestTime>1500||GetAncestor(GetForegroundWindow(),GA_ROOT)!=window(*source))continue;
        std::shared_ptr<View> target;for(auto& v:s.views)if(v->id==command.view&&v->hwnd==window(command)&&v->mixedGroup==command.group)target=v;
        if(!target||!live(command)||!IsWindowEnabled(target->hwnd))continue;
        s.mixedPresentation(target->hwnd,false);
        WINDOWPLACEMENT slot{sizeof(slot)};RECT bounds{};
        if(windowlayout::capture(window(*source),slot,bounds)&&!windowlayout::minimized(slot))windowlayout::apply(target->hwnd,slot,bounds);
        s.activate(target->id,target->hwnd);
        bool activated=GetAncestor(GetForegroundWindow(),GA_ROOT)==target->hwnd;
        logEvent(L"mixed.activate",{{"command",command.command},{"activated",activated}});
        s.lastOperation=activated?L"mixed.activated":L"mixed.activation.blocked";s.lastError=activated?S_OK:E_PENDING;
        if(activated)this->activated(s,target->hwnd);
        if(activated&&command.command==Close&&!target->locked)s.close(target->id);
    }
}
void Client::leave(Session& s,uint64_t id){
    auto key=localKey(s,id);
    store.access([&](Ledger& state){auto m=find(state,key);if(!m||!m->group)return;
        bool belongs=false;for(auto& own:state.members)if(own.owner==owner&&own.group==m->group)belongs=true;
        if(belongs){m->group=0;m->command=None;normalize(state);}
    });tick(s);
}
void Client::stop(Session& s){
    s.restoreMixedPresentation();
    for(auto& m:snapshot)if(m.owner==owner&&live(m))RemovePropW(window(m),windowProperty);
    store.access([&](Ledger& state){for(auto& m:state.members)if(m.owner==owner){if(live(m))RemovePropW(window(m),windowProperty);m={};}normalize(state);});
    for(auto& v:s.views)if(v->mixedGroup){s.restoreView(*v);v->groupId=v->mixedOriginalGroup;v->independent=v->mixedOriginalIndependent;v->mixedGroup=0;}
    snapshot.clear();
}
bool Client::drop(Session& s,uint64_t id,POINT point){
    HWND hit=WindowFromPoint(point);wchar_t name[80]{};GetClassNameW(hit,name,80);
    for(int i=0;hit&&wcscmp(name,L"openOfficetab.Tabs")&&i<8;i++){hit=GetParent(hit);GetClassNameW(hit,name,80);}
    if(wcscmp(name,L"openOfficetab.Tabs"))return false;
    return joinInto(s,id,GetAncestor(hit,GA_ROOT));
}
void Client::activated(Session& s,HWND root){
    s.mixedPresentation(root,false);
    store.access([&](Ledger& state){for(auto& m:state.members)if(m.owner==owner&&window(m)==root&&m.group){auto group=m.group,key=m.key;for(auto& n:state.members)if(n.group==group)n.selected=key;break;}});
}
bool Client::setWidth(Session& s,uint64_t id,int width){
    if(width!=0&&(width<80||width>360))return false;
    auto key=localKey(s,id);bool done=false;
    store.access([&](Ledger& state){auto m=find(state,key);if(!m||!m->group)return;bool shared=false;for(auto& n:state.members)if(n.owner==owner&&n.group==m->group)shared=true;if(shared){m->width=(uint32_t)width;done=true;}});
    if(done){for(auto& m:snapshot)if(m.key==key)m.width=(uint32_t)width;for(auto& v:s.views)if(localKey(s,v->id)==key)v->manualTabWidth=width;s.redraw();}return done;
}
bool Client::joinInto(Session& s,uint64_t id,HWND root){
    tick(s);auto source=localKey(s,id);bool joined=false;
    store.access([&](Ledger& state){normalize(state);uint64_t target=0;for(auto& m:state.members)if(window(m)==root&&m.key!=source)target=m.key;
        if(target)joined=join(state,source,target,GetTickCount64());
    });if(joined)tick(s);return joined;
}
void Client::choose(Session& s,HWND ownerWindow,uint64_t id,POINT point){
    tick(s);auto source=localKey(s,id);if(!source)return;
    std::vector<Member> choices;for(auto& m:snapshot)if(m.key!=source&&available(m,GetTickCount64()))choices.push_back(m);
    UINT command=0;{
        tabmenu::Menu menu(ownerWindow,s.host,s.darkMode(),s.language());
        for(size_t i=0;i<choices.size();i++){auto& m=choices[i];std::wstring label=(m.host==0?L"Word · ":m.host==1?L"Excel · ":L"PowerPoint · ")+std::wstring(m.title,wcsnlen_s(m.title,256));
            std::wstring escaped;for(auto c:label){escaped+=c;if(c==L'&')escaped+=c;}menu.add(menu.handle(),(UINT)i+1,escaped.c_str());}
        if(choices.empty())menu.add(menu.handle(),0,s.tr(L"请先打开另一个 Office 文档"),tabmenu::Glyph::none,false);
        menu.separator();menu.add(menu.handle(),1000,s.tr(L"退出混合标签组"),tabmenu::Glyph::external,contains(GetAncestor(ownerWindow,GA_ROOT)));
        command=menu.track(point);
    }
    if(command==1000)store.access([&](Ledger& state){auto m=find(state,source);if(m&&m->owner==this->owner){m->group=0;m->command=None;}normalize(state);});
    else if(command&&command<=choices.size())store.access([&](Ledger& state){normalize(state);join(state,source,choices[command-1].key,GetTickCount64());});
    tick(s);
}
}
