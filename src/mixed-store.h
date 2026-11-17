#pragma once
#include "com.h"
#include <sddl.h>
#include <cstdint>

// A volatile, logon- and integrity-scoped ledger. No COM objects, process-local
// pointers, file paths, credentials, or persistent workspace data cross it.
namespace mixedoffice {
constexpr uint32_t capacity=96, protocol=1;
// Preserve the existing signed 32-bit Automation argument contract.
constexpr uint64_t remoteBit=uint64_t{1}<<30;
constexpr wchar_t windowProperty[]=L"openOfficeTab.Mixed.v1";
enum Command:uint32_t {None,Activate,Close};
#pragma pack(push,8)
struct Member {
    uint64_t key=0,owner=0,start=0,view=0,window=0,group=0,order=0,seen=0;
    uint64_t request=0,requestSource=0,requestTime=0;
    uint32_t pid=0,host=0,ready=0,saved=1,locked=0,command=0;
    wchar_t title[256]{};
};
struct Ledger {uint32_t version=0,bytes=0,poison=0,reserved=0;uint64_t serial=0;Member members[capacity]{};};
#pragma pack(pop)
static_assert(sizeof(Member)==624,"Mixed Office IPC layout must match x86/x64");
static_assert(sizeof(Ledger)==59928,"Mixed Office IPC layout must match x86/x64");
inline HWND window(const Member& m){return (HWND)(UINT_PTR)m.window;}
inline bool live(const Member& m){
    if(!m.key||!m.pid||!m.window||m.window>UINT32_MAX||m.host>2||!(m.key&remoteBit))return false;
    DWORD pid=0;GetWindowThreadProcessId(window(m),&pid);
    return pid==m.pid&&GetPropW(window(m),windowProperty)==(HANDLE)(UINT_PTR)(m.key&0x7fffffff);
}
inline bool available(const Member& m,uint64_t now){return live(m)&&m.ready&&now>=m.seen&&now-m.seen<2000&&IsWindowEnabled(window(m));}
inline Member* find(Ledger& state,uint64_t key){for(auto& m:state.members)if(key&&m.key==key)return &m;return nullptr;}
inline uint64_t next(Ledger& state){return state.serial<0x3ffffffe?remoteBit|++state.serial:0;}
inline void normalize(Ledger& state){
    for(auto& m:state.members)if(m.key&&!live(m))m={};
    for(auto& m:state.members)if(m.group){unsigned count=0;for(auto& n:state.members)if(n.key&&n.group==m.group)++count;if(count<2)m.group=0;}
}
inline bool join(Ledger& state,uint64_t source,uint64_t target,uint64_t now){
    auto a=find(state,source),b=find(state,target);
    if(!a||!b||a==b||!available(*a,now)||!available(*b,now))return false;
    if(a->group&&a->group==b->group)return true;
    auto group=b->group?b->group:next(state);if(!group)return false;
    b->group=group;if(!b->order)b->order=b->key;
    uint64_t last=0;for(auto& m:state.members)if(m.group==group)last=std::max(last,m.order);
    a->group=group;a->order=last+1;a->command=None;a->request=0;
    normalize(state);return true;
}
inline bool enqueue(Ledger& state,uint64_t source,uint64_t target,Command command,uint64_t now){
    auto a=find(state,source),b=find(state,target);
    if(!a||!b||a==b||!a->group||a->group!=b->group||!available(*a,now)||!available(*b,now)||b->command||
       GetAncestor(GetForegroundWindow(),GA_ROOT)!=window(*a)||(command!=Activate&&command!=Close)||(command==Close&&b->locked))return false;
    b->request=next(state);if(!b->request)return false;
    b->requestSource=a->key;b->requestTime=now;b->command=command;return true;
}
class Store {
    HANDLE mapping=nullptr,mutex=nullptr;Ledger* state=nullptr;bool broken=false;
public:
    // suffix isolates native tests from live Office sessions.
    explicit Store(const std::wstring& suffix=L""){
        HANDLE token=nullptr;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return;
        DWORD bytes=0;GetTokenInformation(token,TokenGroups,nullptr,0,&bytes);std::vector<BYTE> buffer(bytes);
        std::wstring logon;
        if(bytes&&GetTokenInformation(token,TokenGroups,buffer.data(),bytes,&bytes)){
            auto groups=(TOKEN_GROUPS*)buffer.data();for(DWORD i=0;i<groups->GroupCount;i++)if((groups->Groups[i].Attributes&SE_GROUP_LOGON_ID)==SE_GROUP_LOGON_ID){LPWSTR sid=nullptr;if(ConvertSidToStringSidW(groups->Groups[i].Sid,&sid)){logon=sid;LocalFree(sid);}break;}
        }
        BYTE integrity[256]{};DWORD size=sizeof(integrity);DWORD level=0;
        if(GetTokenInformation(token,TokenIntegrityLevel,integrity,size,&size)){auto sid=((TOKEN_MANDATORY_LABEL*)integrity)->Label.Sid;level=*GetSidSubAuthority(sid,*GetSidSubAuthorityCount(sid)-1);}
        CloseHandle(token);if(logon.empty()||!level||suffix.find_first_not_of(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")!=std::wstring::npos)return;
        auto name=L"Local\\openOfficeTab.Mixed.v1."+logon+L"."+std::to_wstring(level)+suffix;
        PSECURITY_DESCRIPTOR descriptor=nullptr;auto acl=L"D:P(A;;GA;;;SY)(A;;GA;;;"+logon+L")";
        if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(),SDDL_REVISION_1,&descriptor,nullptr))return;
        SECURITY_ATTRIBUTES security{sizeof(security),descriptor,FALSE};
        mutex=CreateMutexW(&security,FALSE,(name+L".lock").c_str());
        mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,&security,PAGE_READWRITE,0,sizeof(Ledger),name.c_str());LocalFree(descriptor);
        if(mapping&&mutex)state=(Ledger*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Ledger));
        access([](Ledger&){});
    }
    ~Store(){if(state)UnmapViewOfFile(state);if(mapping)CloseHandle(mapping);if(mutex)CloseHandle(mutex);}
    Store(const Store&)=delete;Store& operator=(const Store&)=delete;
    bool failed()const{return broken||!state||!mutex;}
    bool access(const std::function<void(Ledger&)>& body){
        if(!state||!mutex)return false;
        DWORD wait=WaitForSingleObject(mutex,0);if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED)return false;
        struct Unlock {HANDLE handle;~Unlock(){ReleaseMutex(handle);}} unlock{mutex};
        // An interrupted transaction is never accepted as a coherent group.
        if(wait==WAIT_ABANDONED){state->poison=1;broken=true;return false;}
        if(!state->version&&!state->bytes){*state={};state->version=protocol;state->bytes=sizeof(Ledger);}
        if(state->version!=protocol||state->bytes!=sizeof(Ledger)||state->poison){broken=true;return false;}
        body(*state);return true;
    }
};
}
