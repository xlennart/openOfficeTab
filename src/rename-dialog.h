#pragma once
#include "model.h"

// Native controls retain text editing, accessibility and dialog keyboard behavior.
namespace renameui {
struct RenameChoice {
    Session* session;unsigned long long id;std::wstring path;int language;
    std::wstring name{};
    int scale=96;bool dark=false,contrast=false;
    COLORREF background=0,foreground=0,muted=0,field=0,border=0,accent=0,footer=0;
    HFONT body=nullptr,title=nullptr;HBRUSH backgroundBrush=nullptr,fieldBrush=nullptr;
    ~RenameChoice(){clear();}
    void clear(){if(body)DeleteObject(body);if(title)DeleteObject(title);if(backgroundBrush)DeleteObject(backgroundBrush);if(fieldBrush)DeleteObject(fieldBrush);body=title=nullptr;backgroundBrush=fieldBrush=nullptr;}
    int px(int n)const{return MulDiv(n,scale,96);}
    RECT rect(int x,int y,int w,int h)const{return {px(x),px(y),px(x+w),px(y+h)};}
};
inline void rounded(HDC dc,RECT r,COLORREF fill,COLORREF line,int radius,int thickness=1){
    auto brush=CreateSolidBrush(fill);auto pen=CreatePen(PS_SOLID,thickness,line);
    auto oldBrush=SelectObject(dc,brush);auto oldPen=SelectObject(dc,pen);
    RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);
    SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(pen);DeleteObject(brush);
}
inline COLORREF tint(COLORREF color,bool lighter,int percent){
    auto channel=[&](int value){return (value*(100-percent)+(lighter?255:0)*percent)/100;};
    return RGB(channel(GetRValue(color)),channel(GetGValue(color)),channel(GetBValue(color)));
}
inline LRESULT CALLBACK controlProc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    if(message==WM_SETFOCUS||message==WM_KILLFOCUS){InvalidateRect(GetParent(window),nullptr,FALSE);InvalidateRect(window,nullptr,FALSE);}
    if(message==WM_MOUSEMOVE){TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);InvalidateRect(window,nullptr,FALSE);}
    if(message==WM_MOUSELEAVE)InvalidateRect(window,nullptr,FALSE);
    if(message==WM_NCDESTROY)RemoveWindowSubclass(window,controlProc,1);
    return DefSubclassProc(window,message,wp,lp);
}
inline void layout(HWND dialog,RenameChoice& c){
    c.clear();c.scale=dpi(dialog);
    HIGHCONTRASTW hc{sizeof(hc)};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0);c.contrast=(hc.dwFlags&HCF_HIGHCONTRASTON)!=0;
    c.dark=c.session&&c.session->darkMode();
    c.background=c.dark?RGB(32,32,32):RGB(255,255,255);c.foreground=c.dark?RGB(245,245,245):RGB(32,35,40);
    c.muted=c.dark?RGB(179,184,193):RGB(100,108,120);c.field=c.dark?RGB(43,43,43):RGB(255,255,255);
    c.border=c.dark?RGB(91,94,101):RGB(204,210,219);c.footer=c.dark?RGB(27,27,27):RGB(247,248,250);
    auto host=c.session?c.session->host:Host::Word;
    c.accent=host==Host::Word?RGB(35,91,180):host==Host::Excel?RGB(16,124,65):RGB(196,59,28);
    if(c.dark)c.accent=host==Host::Word?RGB(103,163,247):host==Host::Excel?RGB(82,191,134):RGB(245,142,113);
    if(c.contrast){c.background=c.footer=c.field=GetSysColor(COLOR_WINDOW);c.foreground=c.muted=GetSysColor(COLOR_WINDOWTEXT);c.border=c.foreground;c.accent=GetSysColor(COLOR_HIGHLIGHT);}
    auto face=i18n::resolve(c.language)==1?L"Microsoft YaHei UI":L"Segoe UI";
    c.body=CreateFontW(-c.px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,face);
    c.title=CreateFontW(-c.px(21),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,face);
    c.backgroundBrush=CreateSolidBrush(c.background);c.fieldBrush=CreateSolidBrush(c.field);
    for(HWND child=GetWindow(dialog,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))SendMessageW(child,WM_SETFONT,(WPARAM)(GetDlgCtrlID(child)==110?c.title:c.body),TRUE);
    auto move=[&](int id,int x,int y,int w,int h){MoveWindow(GetDlgItem(dialog,id),c.px(x),c.px(y),c.px(w),c.px(h),TRUE);};
    move(110,80,23,376,30);move(111,80,57,376,22);move(112,24,103,432,22);
    // Extension has its own read-only area; it is never part of the editable stem.
    auto suffix=documentrename::extension(c.path);HDC dc=GetDC(dialog);auto old=SelectObject(dc,c.body);SIZE size{};GetTextExtentPoint32W(dc,suffix.c_str(),(int)suffix.size(),&size);SelectObject(dc,old);ReleaseDC(dialog,dc);
    int badge=std::clamp(MulDiv(size.cx,96,c.scale)+24,54,112);
    move(101,38,139,432-badge-28,24);move(113,456-badge,139,badge-12,24);
    move(114,24,184,432,38);move(IDCANCEL,256,238,92,34);move(IDOK,360,238,96,34);
    InvalidateRect(dialog,nullptr,TRUE);
}
inline INT_PTR CALLBACK renameProc(HWND dialog,UINT message,WPARAM wp,LPARAM lp){
    auto choice=(RenameChoice*)GetWindowLongPtrW(dialog,DWLP_USER);
    if(message==WM_INITDIALOG){
        choice=(RenameChoice*)lp;SetWindowLongPtrW(dialog,DWLP_USER,lp);
        SetWindowTextW(dialog,L"openOfficeTab");
        auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id){return CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,dialog,(HMENU)(INT_PTR)id,module,nullptr);};
        control(L"STATIC",i18n::text(choice->language,L"重命名文件"),SS_NOPREFIX,110);
        control(L"STATIC",documentrename::leaf(choice->path).c_str(),SS_PATHELLIPSIS|SS_NOPREFIX,111);
        control(L"STATIC",i18n::text(choice->language,L"新文件名"),SS_NOPREFIX,112);
        auto edit=control(L"EDIT",documentrename::stem(choice->path).c_str(),WS_TABSTOP|ES_AUTOHSCROLL,101);
        SendMessageW(edit,EM_SETLIMITTEXT,255,0);SetWindowSubclass(edit,controlProc,1,0);
        control(L"STATIC",documentrename::extension(choice->path).c_str(),SS_RIGHT|SS_ENDELLIPSIS|SS_NOPREFIX,113);
        control(L"STATIC",i18n::text(choice->language,L"保存当前编辑，再关闭文档并用新名称重新打开。"),SS_NOPREFIX,114);
        for(int id:{IDCANCEL,IDOK}){auto button=control(L"BUTTON",i18n::text(choice->language,id==IDOK?L"重命名":L"取消"),WS_TABSTOP|BS_OWNERDRAW,id);SetWindowSubclass(button,controlProc,1,0);}
        layout(dialog,*choice);auto& c=*choice;
        RECT r=c.rect(0,0,480,288);AdjustWindowRectEx(&r,(DWORD)GetWindowLongPtrW(dialog,GWL_STYLE),FALSE,(DWORD)GetWindowLongPtrW(dialog,GWL_EXSTYLE));
        RECT parent{};GetWindowRect(GetParent(dialog),&parent);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(GetParent(dialog),MONITOR_DEFAULTTONEAREST),&monitor);
        int width=r.right-r.left,height=r.bottom-r.top;
        SetWindowPos(dialog,nullptr,std::clamp(parent.left+(parent.right-parent.left-width)/2,monitor.rcWork.left,std::max(monitor.rcWork.left,monitor.rcWork.right-width)),std::clamp(parent.top+(parent.bottom-parent.top-height)/2,monitor.rcWork.top,std::max(monitor.rcWork.top,monitor.rcWork.bottom-height)),width,height,SWP_NOZORDER|SWP_NOACTIVATE);
        // Optional dark native caption; load dynamically to keep older Windows support.
        auto dwm=LoadLibraryW(L"dwmapi.dll");if(dwm){using SetAttribute=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);auto set=(SetAttribute)GetProcAddress(dwm,"DwmSetWindowAttribute");BOOL dark=c.dark&&!c.contrast;if(set)set(dialog,20,&dark,sizeof(dark));FreeLibrary(dwm);}
        SendMessageW(dialog,DM_SETDEFID,IDOK,0);SetFocus(edit);SendMessageW(edit,EM_SETSEL,0,-1);return FALSE;
    }
    if(!choice)return FALSE;
    auto& c=*choice;
    if(message==WM_ERASEBKGND)return TRUE;
    if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLOREDIT){
        auto dc=(HDC)wp;int id=GetDlgCtrlID((HWND)lp);SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,(id==111||id==113||id==114)?c.muted:c.foreground);
        SetBkColor(dc,id==101||id==113?c.field:c.background);return (INT_PTR)(id==101||id==113?c.fieldBrush:c.backgroundBrush);
    }
    if(message==WM_PAINT||message==WM_PRINTCLIENT){
        PAINTSTRUCT ps{};HDC dc=message==WM_PAINT?BeginPaint(dialog,&ps):(HDC)wp;RECT bounds{};GetClientRect(dialog,&bounds);FillRect(dc,&bounds,c.backgroundBrush);
        RECT footer=c.rect(0,224,480,64);auto brush=CreateSolidBrush(c.footer);FillRect(dc,&footer,brush);DeleteObject(brush);
        auto border=c.rect(24,130,432,44);rounded(dc,border,c.field,GetFocus()==GetDlgItem(dialog,101)?c.accent:c.border,c.px(12),c.px(GetFocus()==GetDlgItem(dialog,101)?2:1));
        auto icon=c.rect(24,24,40,48);rounded(dc,icon,c.footer,c.border,c.px(8));
        auto pen=CreatePen(PS_SOLID,c.px(2),c.accent);auto oldPen=SelectObject(dc,pen);
        MoveToEx(dc,c.px(35),c.px(40),nullptr);LineTo(dc,c.px(53),c.px(40));MoveToEx(dc,c.px(35),c.px(47),nullptr);LineTo(dc,c.px(49),c.px(47));MoveToEx(dc,c.px(35),c.px(54),nullptr);LineTo(dc,c.px(46),c.px(54));SelectObject(dc,oldPen);DeleteObject(pen);
        if(message==WM_PAINT)EndPaint(dialog,&ps);return TRUE;
    }
    if(message==WM_DRAWITEM&&(wp==IDOK||wp==IDCANCEL)){
        auto item=(DRAWITEMSTRUCT*)lp;auto dc=item->hDC;bool primary=wp==IDOK,disabled=(item->itemState&ODS_DISABLED)!=0,pressed=(item->itemState&ODS_SELECTED)!=0;
        POINT point{};GetCursorPos(&point);ScreenToClient(item->hwndItem,&point);bool hover=PtInRect(&item->rcItem,point)!=FALSE;
        auto fill=primary?c.accent:c.field;
        if(!primary&&(hover||pressed))fill=c.footer;
        if(primary&&(hover||pressed)&&!c.contrast)fill=tint(c.accent,c.dark,pressed?20:10);
        if(disabled)fill=c.border;
        auto brush=CreateSolidBrush(c.footer);FillRect(dc,&item->rcItem,brush);DeleteObject(brush);
        RECT r=item->rcItem;InflateRect(&r,-c.px(1),-c.px(1));rounded(dc,r,fill,primary?fill:c.border,c.px(10));
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,disabled?c.muted:primary?(c.contrast?GetSysColor(COLOR_HIGHLIGHTTEXT):c.dark?RGB(20,26,34):RGB(255,255,255)):c.foreground);
        auto font=SelectObject(dc,c.body);wchar_t text[80]{};GetWindowTextW(item->hwndItem,text,80);DrawTextW(dc,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(dc,font);
        if((item->itemState&ODS_FOCUS)&&!(item->itemState&ODS_NOFOCUSRECT)){InflateRect(&r,-c.px(4),-c.px(4));DrawFocusRect(dc,&r);}return TRUE;
    }
    if(message==WM_DPICHANGED){auto r=(RECT*)lp;SetWindowPos(dialog,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);layout(dialog,c);return TRUE;}
    if(message==WM_COMMAND&&LOWORD(wp)==IDOK){
        wchar_t name[256]{};GetDlgItemTextW(dialog,101,name,256);
        auto alert=[&](const wchar_t* key){MessageBoxW(dialog,i18n::text(c.language,key),i18n::text(c.language,L"重命名文件"),MB_OK|MB_ICONINFORMATION);};
        if(!documentrename::validStem(name,documentrename::extension(c.path))){alert(L"文件名无效。请勿使用保留名称、路径字符或末尾的空格和句点。");return TRUE;}
        auto view=c.session->find(c.id);
        if(!view||view->path!=c.path){alert(L"文档路径已改变，请关闭对话框后重试。");return TRUE;}
        c.name=name;
        EndDialog(dialog,IDOK);return TRUE;
    }
    if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(wp)==IDCANCEL)){EndDialog(dialog,IDCANCEL);return TRUE;}
    return FALSE;
}
}
