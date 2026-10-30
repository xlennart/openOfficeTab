#pragma once
#include "com.h"
#include "diagnostic-trace.h"

namespace windowlayout {
// WINDOWPLACEMENT keeps the pre-snap restore rectangle on some Windows builds.
// Keep screen bounds separately, in physical pixels, for the visible tab slot.
class PhysicalCoordinates {
    DPI_AWARENESS_CONTEXT previous;
public:
    PhysicalCoordinates():previous(SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)){}
    ~PhysicalCoordinates(){if(previous)SetThreadDpiAwarenessContext(previous);}
};
inline bool capture(HWND window,WINDOWPLACEMENT& placement,RECT& bounds){
    PhysicalCoordinates context;placement.length=sizeof(placement);
    return GetWindowPlacement(window,&placement)&&GetWindowRect(window,&bounds);
}
inline bool minimized(const WINDOWPLACEMENT& placement){return placement.showCmd==SW_SHOWMINIMIZED||placement.showCmd==SW_MINIMIZE||placement.showCmd==SW_SHOWMINNOACTIVE;}
inline bool matches(HWND window,const WINDOWPLACEMENT& placement,const RECT& bounds){
    WINDOWPLACEMENT now{sizeof(now)};RECT rect{};
    return capture(window,now,rect)&&now.showCmd==placement.showCmd&&EqualRect(&now.rcNormalPosition,&placement.rcNormalPosition)&&EqualRect(&rect,&bounds);
}
inline bool apply(HWND window,const WINDOWPLACEMENT& placement,const RECT& bounds){
    diagnostic::Scope cause(diagnostic::Align);
    PhysicalCoordinates context;
    if(placement.showCmd==SW_SHOWMAXIMIZED){auto desired=placement;desired.flags=0;return diagnostic::perform(6,window,[&]{BOOL ok=SetWindowPlacement(window,&desired);if(!ok){auto error=GetLastError();logEvent(L"window.api.win32",{{"api",6},{"win32",error}});}return ok!=FALSE;});}
    if(minimized(placement)||bounds.right<=bounds.left||bounds.bottom<=bounds.top)return false;
    // SW_SHOWNOACTIVATE clears maximize/minimize without foreground activation.
    if(IsZoomed(window)||IsIconic(window))diagnostic::show(window,SW_SHOWNOACTIVATE);
    return diagnostic::position(window,nullptr,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOZORDER|SWP_NOACTIVATE);
}
inline bool sameSlot(HWND window,const WINDOWPLACEMENT& placement,const RECT& bounds){
    WINDOWPLACEMENT now{sizeof(now)};RECT rect{};
    if(!capture(window,now,rect))return false;
    return placement.showCmd==SW_SHOWMAXIMIZED ? now.showCmd==SW_SHOWMAXIMIZED&&EqualRect(&rect,&bounds) : !IsIconic(window)&&!IsZoomed(window)&&EqualRect(&rect,&bounds);
}
inline bool ribbonInitializing(HWND host){
    PhysicalCoordinates coordinates;RECT client{};
    if(!GetClientRect(host,&client))return false;MapWindowPoints(host,nullptr,(POINT*)&client,2);
    struct Check {RECT client;bool initializing=false;};Check check{client};
    EnumChildWindows(host,[](HWND bar,LPARAM data)->BOOL{
        wchar_t cls[128]{};GetClassNameW(bar,cls,128);
        if(_wcsicmp(cls,L"MsoCommandBar")||!IsWindowVisible(bar))return TRUE;
        RECT bounds{};if(!GetWindowRect(bar,&bounds)||bounds.bottom-bounds.top<=MulDiv(96,dpi(bar),96))return TRUE;
        const auto& area=((Check*)data)->client;
        if(abs(bounds.top-area.top)>MulDiv(3,dpi(bar),96)||bounds.right-bounds.left<(area.right-area.left)*3/4)return TRUE;
        // A visible Ribbon frame is insufficient: Office temporarily parks its
        // actual NetUI tree under an invisible MsoWorkPane during activation.
        struct Content {RECT bounds;bool ready=false;} content{bounds};
        EnumChildWindows(bar,[](HWND child,LPARAM value)->BOOL{
            auto& c=*(Content*)value;wchar_t name[128]{};GetClassNameW(child,name,128);
            if(_wcsicmp(name,L"NetUIHWND")||!IsWindowVisible(child))return TRUE;
            RECT r{},intersection{};
            if(GetWindowRect(child,&r)&&IntersectRect(&intersection,&r,&c.bounds)&&
               intersection.right-intersection.left>=(c.bounds.right-c.bounds.left)*3/4&&
               intersection.bottom-intersection.top>=(c.bounds.bottom-c.bounds.top)*3/4){c.ready=true;return FALSE;}
            return TRUE;
        },(LPARAM)&content);
        if(!content.ready)((Check*)data)->initializing=true;
        return TRUE;
    },(LPARAM)&check);
    // No expanded Ribbon is valid (collapsed Ribbon / fullscreen); do not
    // force it open or change a user's preferred display mode.
    return check.initializing;
}
inline bool reserveFrame(HWND frame,HWND ole,HWND host){
    if(!frame||!ole||!host||!IsWindow(frame)||!IsWindow(ole)||frame==host||!IsChild(frame,ole)||GetAncestor(frame,GA_ROOT)!=host)return false;
    wchar_t name[128]{};GetClassNameW(frame,name,128);if(_wcsicmp(name,L"MsoCommandBar"))return false;
    PhysicalCoordinates context;RECT bounds{};
    // Ribbon and status bars use the same MsoCommandBar/MsoWorkPane classes.
    // Only compact our short CTP strip, never a large/shared Office toolbar.
    return GetWindowRect(frame,&bounds)&&bounds.right>bounds.left&&bounds.bottom>bounds.top&&bounds.bottom-bounds.top<=MulDiv(96,dpi(frame),96);
}
inline bool seamBounds(const RECT& reserved,const RECT& ribbon,int scale,RECT& result){
    int edge=std::max(1,MulDiv(3,scale,96));
    if(reserved.right<=reserved.left||ribbon.bottom-ribbon.top<MulDiv(96,scale,96)||abs(ribbon.bottom-reserved.top)>edge)return false;
    int left=std::max(ribbon.left,reserved.left),right=std::min(ribbon.right,reserved.right);if(right-left<(reserved.right-reserved.left)*9/10)return false;
    result={left,ribbon.bottom-edge,right,ribbon.bottom};return true;
}
inline bool seamRect(HWND frame,HWND ole,HWND host,RECT& result){
    if(!reserveFrame(frame,ole,host))return false;
    PhysicalCoordinates context;RECT reserved{};if(!GetWindowRect(frame,&reserved))return false;
    HWND parent=GetParent(frame);
    // The full ribbon is a sibling in this dock. Touch only its final 3 DIP,
    // and reject collapsed ribbons, foreign roots and non-adjacent toolbars.
    for(HWND candidate=GetWindow(parent,GW_CHILD);candidate;candidate=GetWindow(candidate,GW_HWNDNEXT)){
        if(candidate==frame||GetAncestor(candidate,GA_ROOT)!=host||!IsWindowVisible(candidate)||GetWindowThreadProcessId(candidate,nullptr)!=GetCurrentThreadId())continue;
        wchar_t cls[128]{};GetClassNameW(candidate,cls,128);if(_wcsicmp(cls,L"MsoCommandBar"))continue;
        RECT ribbon{};if(!GetWindowRect(candidate,&ribbon)||!seamBounds(reserved,ribbon,dpi(host),result))continue;
        MapWindowPoints(nullptr,host,(POINT*)&result,2);return true;
    }return false;
}
}
