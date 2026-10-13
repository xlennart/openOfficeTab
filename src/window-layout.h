#pragma once
#include "com.h"

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
    PhysicalCoordinates context;
    if(placement.showCmd==SW_SHOWMAXIMIZED){auto desired=placement;desired.flags=0;return SetWindowPlacement(window,&desired)!=FALSE;}
    if(minimized(placement)||bounds.right<=bounds.left||bounds.bottom<=bounds.top)return false;
    // SW_SHOWNOACTIVATE clears maximize/minimize without foreground activation.
    if(IsZoomed(window)||IsIconic(window))ShowWindow(window,SW_SHOWNOACTIVATE);
    return SetWindowPos(window,nullptr,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE;
}
inline bool sameSlot(HWND window,const WINDOWPLACEMENT& placement,const RECT& bounds){
    WINDOWPLACEMENT now{sizeof(now)};RECT rect{};
    if(!capture(window,now,rect))return false;
    return placement.showCmd==SW_SHOWMAXIMIZED ? now.showCmd==SW_SHOWMAXIMIZED&&EqualRect(&rect,&bounds) : !IsIconic(window)&&!IsZoomed(window)&&EqualRect(&rect,&bounds);
}
inline bool reserveFrame(HWND frame,HWND ole,HWND host){
    if(!frame||!ole||!host||!IsWindow(frame)||!IsWindow(ole)||frame==host||!IsChild(frame,ole)||GetAncestor(frame,GA_ROOT)!=host)return false;
    wchar_t name[128]{};GetClassNameW(frame,name,128);if(_wcsicmp(name,L"MsoCommandBar"))return false;
    PhysicalCoordinates context;RECT bounds{};
    // Ribbon and status bars use the same MsoCommandBar/MsoWorkPane classes.
    // Only compact our short CTP strip, never a large/shared Office toolbar.
    return GetWindowRect(frame,&bounds)&&bounds.right>bounds.left&&bounds.bottom>bounds.top&&bounds.bottom-bounds.top<=MulDiv(96,dpi(frame),96);
}
}
