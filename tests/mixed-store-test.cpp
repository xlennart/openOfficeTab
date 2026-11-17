#include "../src/mixed-store.h"
#include <cassert>
#include <iostream>
using namespace mixedoffice;
int wmain(int argc,wchar_t** argv){
    if(argc==3&&!wcscmp(argv[1],L"--peer")){
        Store peer(argv[2]);bool checked=false;
        assert(peer.access([&](Ledger& state){assert(state.serial==17);assert(state.members[0].view==0x12345678abcdef01ULL);state.reserved=sizeof(void*)*8;state.serial=18;checked=true;}));
        return checked?0:2;
    }
    auto suffix=L"test-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    Store first(suffix),second(suffix);assert(!first.failed()&&!second.failed());
    assert(first.access([](Ledger& state){state.serial=17;state.members[0].view=0x12345678abcdef01ULL;}));
    std::wstring executable(32768,L'\0');auto length=GetModuleFileNameW(nullptr,executable.data(),(DWORD)executable.size());executable.resize(length);
    std::wstring command=L"\""+(argc==2?std::wstring(argv[1]):executable)+L"\" --peer "+suffix;
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
    assert(CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process));
    assert(WaitForSingleObject(process.hProcess,10000)==WAIT_OBJECT_0);DWORD code=1;assert(GetExitCodeProcess(process.hProcess,&code)&&!code);CloseHandle(process.hThread);CloseHandle(process.hProcess);
    assert(second.access([&](Ledger& state){assert(state.serial==18);assert(state.reserved==32||state.reserved==64);state.members[0]={};}));
    HWND a=CreateWindowExW(0,L"STATIC",L"A",WS_OVERLAPPEDWINDOW,0,0,200,100,nullptr,nullptr,nullptr,nullptr);
    HWND b=CreateWindowExW(0,L"STATIC",L"B",WS_OVERLAPPEDWINDOW,0,0,200,100,nullptr,nullptr,nullptr,nullptr);
    HWND c=CreateWindowExW(0,L"STATIC",L"C",WS_OVERLAPPEDWINDOW,0,0,200,100,nullptr,nullptr,nullptr,nullptr);
    assert(a&&b);
    assert(first.access([&](Ledger& state){
        auto init=[&](Member& m,HWND window,unsigned host){m={};m.key=next(state);m.window=(uint64_t)(UINT_PTR)window;m.pid=GetCurrentProcessId();m.owner=host+1;m.host=host;m.ready=1;m.seen=GetTickCount64();assert(SetPropW(window,windowProperty,(HANDLE)(UINT_PTR)(m.key&0x7fffffff)));};
        init(state.members[0],a,0);init(state.members[1],b,1);auto& left=state.members[0];auto& right=state.members[1];
        assert(join(state,left.key,right.key,GetTickCount64()));assert(left.group&&left.group==right.group);
        init(state.members[2],c,2);auto& third=state.members[2];
        // Dragging an existing two-member group onto the third app merges it;
        // it must not strand the other member in the original group.
        assert(join(state,left.key,third.key,GetTickCount64()));assert(left.group==right.group&&right.group==third.group);
        assert(left.selected==third.key&&right.selected==third.key);
        RemovePropW(c,windowProperty);normalize(state);
        right.ready=0;assert(!enqueue(state,left.key,right.key,Activate,GetTickCount64()));right.ready=1;
        right.locked=1;assert(!enqueue(state,left.key,right.key,Close,GetTickCount64()));right.locked=0;
        assert(!join(state,left.key,left.key,GetTickCount64()));
        right.seen=GetTickCount64()-3000;assert(!available(right,GetTickCount64()));right.seen=GetTickCount64();
        // A valid HWND whose lifetime property changed must not be restored or commanded.
        assert(SetPropW(b,windowProperty,(HANDLE)(UINT_PTR)123456));assert(!live(right));normalize(state);assert(!right.key&&!left.group);
        RemovePropW(a,windowProperty);RemovePropW(b,windowProperty);
    }));
    DestroyWindow(a);DestroyWindow(b);DestroyWindow(c);
    assert(first.access([](Ledger& state){state.poison=1;}));
    assert(!second.access([](Ledger&){assert(false);})&&second.failed());
    std::wcout<<L"Mixed ledger: cross-process ABI, membership, busy/locked guards, stale identity and fail-closed passed ("<<sizeof(void*)*8<<L" bit).\n";
}
