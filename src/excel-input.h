#pragma once
#include "com.h"
#include <cstring>

namespace excelinput {
// Minimal ABI from Microsoft's Excel SDK xlcall.h. We read only the mode;
// formula text remains owned by Excel and is never copied or changed.
struct FormulaInfo {int mode=0,length=0;char* text=nullptr;int first=0,last=0,caret=0;};
using PenHelper=long (WINAPI*)(int,void*);
inline PenHelper helper(){
    static PenHelper entry=[]()->PenHelper{
        auto excel=GetModuleHandleW(L"EXCEL.EXE");if(!excel)return nullptr;
        auto library=GetModuleHandleW(L"XLCALL32.DLL");
        if(!library){
            wchar_t path[MAX_PATH]{};auto length=GetModuleFileNameW(excel,path,MAX_PATH);
            if(!length||length>=MAX_PATH)return nullptr;
            auto slash=wcsrchr(path,L'\\');if(!slash)return nullptr;
            std::wstring full(path,slash+1);full+=L"XLCALL32.DLL";
            // Load Excel's own callback library, never search the working folder.
            library=LoadLibraryExW(full.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        }
        if(!library)return nullptr;
        auto address=GetProcAddress(library,"LPenHelper");PenHelper result=nullptr;
        static_assert(sizeof(result)==sizeof(address));memcpy(&result,&address,sizeof(result));return result;
    }();return entry;
}
inline int mode(HWND root,PenHelper entry){
    DWORD process=0;if(!root||GetWindowThreadProcessId(root,&process)!=GetCurrentThreadId()||process!=GetCurrentProcessId()||!entry)return -1;
    FormulaInfo info{};if(entry(0x400e,&info)!=0)return -1;
    return info.mode==0||info.mode==1||info.mode==2||info.mode==4?info.mode:-1;
}
inline int mode(HWND root){return mode(root,helper());}
inline bool editing(HWND root){return mode(root)>0;}
inline bool showReference(HWND source,HWND target){
    if(!source||!target||!IsWindowEnabled(source)||!IsWindowEnabled(target)||IsIconic(target))return false;
    DWORD sourceProcess=0,targetProcess=0;
    if(GetWindowThreadProcessId(source,&sourceProcess)!=GetCurrentThreadId()||GetWindowThreadProcessId(target,&targetProcess)!=GetCurrentThreadId()||sourceProcess!=GetCurrentProcessId()||targetProcess!=sourceProcess)return false;
    // Let Excel's native frame activation preserve its formula owner/point
    // mode, just as switching through Windows does. Window.Activate and an
    // extra SetActiveWindow/SetFocus are not equivalent during formula entry.
    return source==target||SetForegroundWindow(target)!=FALSE;
}
}
