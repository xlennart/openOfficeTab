#pragma once
#include "com.h"

// Process-local numeric causes, never document identities or user strings.
namespace diagnostic {
enum Origin {Unknown=0,Tab=1,Layout=2,Refresh=3,WindowMessage=4,Paint=5,Align=6,Restore=7};
enum Api {ActiveWindow=1,ForegroundWindow=2,WindowPosition=3,WindowShow=4,ComActivate=5};
inline long long window(HWND value){DWORD pid=0;return value&&GetWindowThreadProcessId(value,&pid)&&pid==GetCurrentProcessId()?(long long)(UINT_PTR)value:0;}
struct Gate {
    ULONGLONG last=0;unsigned suppressed=0;bool initialized=false;
    bool take(ULONGLONG now,ULONGLONG interval){if(initialized&&now-last<interval){++suppressed;return false;}last=now;initialized=true;return true;}
    unsigned drain(){auto count=suppressed;suppressed=0;return count;}
};
class Scope {
    runtimelog::TraceContext previous;bool recorded;
public:
    explicit Scope(int origin,long long parent=0,bool record=true,long long job=0):previous(runtimelog::traceContext),recorded(record){
        runtimelog::traceContext={InterlockedIncrement64(&runtimelog::traceCounter),parent?parent:previous.id,origin,job?job:previous.job};
        if(recorded)log(L"trace.begin");
    }
    ~Scope(){if(recorded)log(L"trace.end");runtimelog::traceContext=previous;}
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};
template<class F> inline bool perform(int api,HWND target,F operation){
    logEvent(L"window.api.begin",{{"api",api},{"targetHwnd",window(target)},{"foregroundHwnd",window(GetForegroundWindow())},{"activeHwnd",window(GetActiveWindow())}});
    try{
        bool outcome=operation();
        logEvent(L"window.api.end",{{"api",api},{"targetHwnd",window(target)},{"outcome",outcome},{"foregroundHwnd",window(GetForegroundWindow())},{"activeHwnd",window(GetActiveWindow())}});
        return outcome;
    }catch(const ComError& error){logEvent(L"window.api.error",{{"api",api},{"targetHwnd",window(target)}},error.hr);throw;}
    catch(...){logEvent(L"window.api.error",{{"api",api},{"targetHwnd",window(target)}},E_FAIL);throw;}
}
inline void active(HWND target){perform(ActiveWindow,target,[&]{SetActiveWindow(target);return GetActiveWindow()==target;});}
inline void foreground(HWND target){perform(ForegroundWindow,target,[&]{return SetForegroundWindow(target)!=FALSE;});}
inline bool position(HWND target,HWND after,int x,int y,int w,int h,UINT flags){return perform(WindowPosition,target,[&]{logEvent(L"window.geometry.request",{{"targetHwnd",window(target)},{"afterHwnd",window(after)},{"flags",flags},{"x",x},{"y",y},{"width",w},{"height",h}});BOOL ok=SetWindowPos(target,after,x,y,w,h,flags);if(!ok){auto error=GetLastError();logEvent(L"window.api.win32",{{"api",WindowPosition},{"win32",error}});}return ok!=FALSE;});}
inline void show(HWND target,int command){perform(WindowShow,target,[&]{logEvent(L"window.show.request",{{"command",command},{"targetHwnd",window(target)},{"minimized",IsIconic(target)!=FALSE}});ShowWindow(target,command);return IsWindow(target)!=FALSE;});}
}
