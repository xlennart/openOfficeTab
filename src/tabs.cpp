#include "tabs.h"
#include "layout.h"
#include "window-layout.h"
#include "tab-color.h"
#include <windowsx.h>

HRESULT Tabs::QueryInterface(REFIID i,void** p){
    if(!p)return E_POINTER;*p=nullptr;
    if(i==IID_IUnknown||i==IID_IDispatch)*p=static_cast<IDispatch*>(this);
    else if(i==IID_IOleObject)*p=static_cast<IOleObject*>(this);
    else if(i==IID_IOleWindow||i==IID_IOleInPlaceObject)*p=static_cast<IOleInPlaceObject*>(this);
    else if(i==IID_IOleInPlaceActiveObject)*p=static_cast<IOleInPlaceActiveObject*>(this);
    else if(i==IID_IOleControl)*p=static_cast<IOleControl*>(this);
    else if(i==IID_IViewObject||i==IID_IViewObject2)*p=static_cast<IViewObject2*>(this);
    else if(i==IID_IPersist||i==IID_IPersistStream||i==IID_IPersistStreamInit)*p=static_cast<IPersistStreamInit*>(this);
    else if(i==__uuidof(ITabBinding))*p=static_cast<ITabBinding*>(this);
    if(!*p)return E_NOINTERFACE;AddRef();return S_OK;
}
void Tabs::destroy(){
    session=nullptr;
    // Destruction can start at refcount zero. Detach window callbacks before
    // DestroyWindow dispatches WM_NCDESTROY; proc must not AddRef/Release an
    // already destructing control and delete it a second time.
    if(hwnd&&IsWindow(hwnd))SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);
    if(oleHwnd&&IsWindow(oleHwnd))SetWindowLongPtrW(oleHwnd,GWLP_USERDATA,0);
    if(seamWindow&&IsWindow(seamWindow))SetWindowLongPtrW(seamWindow,GWLP_USERDATA,0);
    Unmount();if(oleHwnd&&IsWindow(oleHwnd))DestroyWindow(oleHwnd);oleHwnd=nullptr;tooltip=nullptr;
}
bool Tabs::Mount(HWND frame){
    if(!frame){compactFrame=nullptr;if(seamWindow)ShowWindow(seamWindow,SW_HIDE);return true;}
    if(!windowlayout::reserveFrame(frame,oleHwnd,hostWindow))return false;
    RECT reserved{};if(!GetWindowRect(frame,&reserved)||reserved.right<=reserved.left||reserved.bottom<=reserved.top)return false;
    if(compactFrame==frame){layout();return true;}
    // Keep rendering independent of Office's replaceable OLE/pane containers.
    // The root child survives a pane rebuild; CTP continues to reserve space.
    if(!hwnd){
        auto context=SetThreadDpiAwarenessContext(GetWindowDpiAwarenessContext(hostWindow));
        hwnd=CreateWindowExW(0,L"openOfficetab.Tabs",i18n::text(session?session->language():0,L"文档标签"),WS_CHILD|WS_CLIPSIBLINGS|WS_TABSTOP,0,0,1,1,hostWindow,nullptr,module,this);
        if(context)SetThreadDpiAwarenessContext(context);
        if(!hwnd){log(L"chrome.displayCreate.failed",HRESULT_FROM_WIN32(GetLastError()));return false;}
        if(tooltip)DestroyWindow(tooltip);
        tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,hwnd,nullptr,module,nullptr);
        TOOLINFOW t{sizeof(t)};t.uFlags=TTF_IDISHWND|TTF_SUBCLASS;t.hwnd=hwnd;t.uId=(UINT_PTR)hwnd;t.lpszText=LPSTR_TEXTCALLBACKW;SendMessageW(tooltip,TTM_ADDTOOLW,0,(LPARAM)&t);SendMessageW(tooltip,TTM_SETMAXTIPWIDTH,0,700);
    }
    compactFrame=frame;ShowWindow(oleHwnd,SW_HIDE);
    layout();ShowWindow(hwnd,session&&session->showTabs(hostWindow)?SW_SHOWNOACTIVATE:SW_HIDE);return true;
}
void Tabs::Unmount(){
    if(windowlayout::reserveFrame(compactFrame,oleHwnd,hostWindow))ShowWindow(compactFrame,SW_SHOWNOACTIVATE);
    compactFrame=nullptr;
    if(seamWindow&&IsWindow(seamWindow)){SetWindowLongPtrW(seamWindow,GWLP_USERDATA,0);DestroyWindow(seamWindow);}seamWindow=nullptr;
    if(tooltip)DestroyWindow(tooltip);tooltip=nullptr;if(hwnd&&IsWindow(hwnd))DestroyWindow(hwnd);hwnd=nullptr;
}
void Tabs::layout(){if(hwnd&&compactFrame&&IsWindow(compactFrame)&&!layingOut){
    if(!windowlayout::reserveFrame(compactFrame,oleHwnd,hostWindow)){if(seamWindow)ShowWindow(seamWindow,SW_HIDE);ShowWindow(hwnd,SW_HIDE);return;}
    layingOut=true;
    auto context=SetThreadDpiAwarenessContext(GetWindowDpiAwarenessContext(hostWindow));RECT r{},current{};GetWindowRect(compactFrame,&r);MapWindowPoints(nullptr,hostWindow,(POINT*)&r,2);GetWindowRect(hwnd,&current);MapWindowPoints(nullptr,hostWindow,(POINT*)&current,2);
    if(!EqualRect(&r,&current))SetWindowPos(hwnd,HWND_TOP,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE);
    else if(::GetWindow(hostWindow,GW_CHILD)!=hwnd&&::GetWindow(hostWindow,GW_CHILD)!=seamWindow)SetWindowPos(hwnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    if(windowlayout::reserveFrame(compactFrame,oleHwnd,hostWindow)&&(GetWindowLongPtrW(compactFrame,GWL_STYLE)&WS_VISIBLE))ShowWindow(compactFrame,SW_HIDE);
    layoutSeam();
    if(context)SetThreadDpiAwarenessContext(context);
    layingOut=false;
}}
void Tabs::layoutSeam(){
    RECT r{};HIGHCONTRASTW contrast{sizeof(contrast)};SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(contrast),&contrast,0);
    bool show=session&&session->settings.barStyle==1&&session->settings.position==1&&session->showTabs(hostWindow)&&!(contrast.dwFlags&HCF_HIGHCONTRASTON)&&windowlayout::seamRect(compactFrame,oleHwnd,hostWindow,r);
    if(!show){if(seamWindow)ShowWindow(seamWindow,SW_HIDE);return;}
    if(!seamWindow){WNDCLASSEXW c{sizeof(c)};c.hInstance=module;c.lpszClassName=L"openOfficetab.Seam";c.lpfnWndProc=seamProc;RegisterClassExW(&c);
        seamWindow=CreateWindowExW(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE,c.lpszClassName,L"",WS_CHILD|WS_CLIPSIBLINGS,r.left,r.top,r.right-r.left,r.bottom-r.top,hostWindow,nullptr,module,this);}
    if(seamWindow){RECT current{};GetWindowRect(seamWindow,&current);MapWindowPoints(nullptr,hostWindow,(POINT*)&current,2);
        if(!EqualRect(&r,&current))SetWindowPos(seamWindow,HWND_TOP,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE);
        else{HWND first=::GetWindow(hostWindow,GW_CHILD);if(first!=seamWindow&&!(first==hwnd&&::GetWindow(hwnd,GW_HWNDNEXT)==seamWindow))SetWindowPos(seamWindow,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);}
        ShowWindow(seamWindow,SW_SHOWNOACTIVATE);InvalidateRect(seamWindow,nullptr,FALSE);}
}
LRESULT CALLBACK Tabs::seamProc(HWND w,UINT message,WPARAM wp,LPARAM lp){
    auto self=(Tabs*)GetWindowLongPtrW(w,GWLP_USERDATA);if(message==WM_NCCREATE){self=(Tabs*)((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)self);}
    if(message==WM_NCHITTEST)return HTTRANSPARENT;if(message==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(message==WM_ERASEBKGND)return 1;
    if(message==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT rect{};GetClientRect(w,&rect);COLORREF background=self&&self->session?self->session->surroundColor():tabcolor::surround(false);HBRUSH brush=CreateSolidBrush(background);FillRect(dc,&rect,brush);DeleteObject(brush);EndPaint(w,&ps);return 0;}
    if(message==WM_NCDESTROY){if(self&&self->seamWindow==w)self->seamWindow=nullptr;SetWindowLongPtrW(w,GWLP_USERDATA,0);}
    return DefWindowProcW(w,message,wp,lp);
}
HRESULT Tabs::activate(HWND parent,LPCRECT supplied,bool focus){
    if(!site)return E_FAIL;
    if(!inPlace)inPlace=site.as<IOleInPlaceSite>();
    if(!inPlace)return E_NOINTERFACE;
    RECT pos=supplied?*supplied:RECT{0,0,700,40},clip=pos;
    if(!activated){HRESULT hr=inPlace->CanInPlaceActivate();if(FAILED(hr))return hr;inPlace->OnInPlaceActivate();activated=true;}
    Ptr<IOleInPlaceFrame> frame;Ptr<IOleInPlaceUIWindow> doc;
    OLEINPLACEFRAMEINFO fi{sizeof(fi)};
    inPlace->GetWindowContext(frame.out(),doc.out(),&pos,&clip,&fi);
    if(!parent)inPlace->GetWindow(&parent);
    if(!oleHwnd){
        WNDCLASSEXW c{sizeof(c)};c.lpfnWndProc=proc;c.hInstance=module;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.lpszClassName=L"openOfficetab.Tabs";c.style=CS_DBLCLKS;RegisterClassExW(&c);
        oleHwnd=CreateWindowExW(0,c.lpszClassName,i18n::text(session?session->language():0,L"文档标签"),WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|WS_TABSTOP,pos.left,pos.top,pos.right-pos.left,pos.bottom-pos.top,parent,nullptr,module,this);
        if(!oleHwnd)return HRESULT_FROM_WIN32(GetLastError());
    }else{if(!compactFrame)SetParent(oleHwnd,parent);SetObjectRects(&pos,&clip);if(!hwnd)ShowWindow(oleHwnd,SW_SHOW);}
    site->ShowObject();
    // The OLE window only reserves space. Requesting UI activation asks Office
    // to remove its document menus/tools, although this control has none to
    // replace them with. Keyboard focus belongs to our separate native child.
    if(focus&&hwnd)SetFocus(hwnd);
    // Office may activate the OLE window after Visible/Add has returned. Do not
    // leave the first layout waiting for the 750 ms recovery timer.
    if(session)session->defer();
    return S_OK;
}
HRESULT Tabs::DoVerb(LONG v,LPMSG,IOleClientSite* s,LONG,HWND p,LPCRECT r){
    if(s)SetClientSite(s);
    if(v==OLEIVERB_HIDE){if(oleHwnd)ShowWindow(oleHwnd,SW_HIDE);UIDeactivate();return S_OK;}
    if(v==OLEIVERB_SHOW||v==OLEIVERB_INPLACEACTIVATE||v==OLEIVERB_PRIMARY||v==OLEIVERB_UIACTIVATE)return activate(p,r,v==OLEIVERB_UIACTIVATE);
    return OLEOBJ_E_NOVERBS;
}
HRESULT Tabs::InPlaceDeactivate(){UIDeactivate();if(oleHwnd)ShowWindow(oleHwnd,SW_HIDE);if(activated&&inPlace)inPlace->OnInPlaceDeactivate();activated=false;inPlace.reset();return S_OK;}
HRESULT Tabs::TranslateAccelerator(LPMSG m){
    if(!m||!session||m->message!=WM_KEYDOWN)return S_FALSE;
    if(m->wParam==VK_LEFT||m->wParam==VK_RIGHT){auto v=session->tabViews(hostWindow);if(v.empty())return S_FALSE;size_t idx=0;for(size_t i=0;i<v.size();i++)if(v[i]->id==session->activeTabId(hostWindow))idx=i;idx=m->wParam==VK_RIGHT?(idx+1)%v.size():(idx+v.size()-1)%v.size();session->activate(v[idx]->id,hostWindow);return S_OK;}
    if(m->wParam==VK_DELETE){session->close(session->activeTabId(hostWindow));return S_OK;}
    if(m->wParam==VK_ESCAPE){finishResize(true);pressed=0;dragging=false;if(GetCapture()==hwnd)ReleaseCapture();auto v=session->find(session->activeId);if(v)SetFocus(v->hwnd);return S_OK;}
    return S_FALSE;
}
unsigned long long Tabs::at(POINT p)const{for(auto& x:hit)if(PtInRect(&x.second,p))return x.first;return 0;}
unsigned long long Tabs::edgeAt(POINT p)const{
    int grip=std::max(3,MulDiv(4,dpi(hwnd),96));
    for(auto& x:hit){RECT r{x.second.right-grip,x.second.top,x.second.right+grip,x.second.bottom};if(PtInRect(&r,p))return x.first;}
    return 0;
}
void Tabs::finishResize(bool cancel){
    auto id=resizing;resizing=0;
    if(cancel&&session){auto v=session->find(id);if(v){v->manualTabWidth=resizeOriginalWidth;session->redraw();}}
    if(id&&!cancel&&session){auto view=session->find(id);if(view)session->persistView(*view);}
    if(id&&hwnd&&GetCapture()==hwnd)ReleaseCapture();
    if(id&&hwnd&&GetFocus()==hwnd&&IsWindow(resizePreviousFocus))SetFocus(resizePreviousFocus);
    if(id)resizePreviousFocus=nullptr;
}
static void fill(HDC d,RECT r,COLORREF c){HBRUSH b=CreateSolidBrush(c);FillRect(d,&r,b);DeleteObject(b);}
static void text(HDC d,std::wstring s,RECT r,COLORREF c,UINT flags=DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX){SetTextColor(d,c);DrawTextW(d,s.c_str(),(int)s.size(),&r,flags);}
static Gdiplus::Color color(COLORREF c){return Gdiplus::Color(255,GetRValue(c),GetGValue(c),GetBValue(c));}
static void rounded(HDC d,RECT r,int diameter,COLORREF bg,COLORREF border,bool outline=false){
    using namespace Gdiplus;
    if(r.right<=r.left||r.bottom<=r.top)return;
    Graphics g(d);g.SetSmoothingMode(SmoothingModeAntiAlias);GraphicsPath path;
    REAL x=(REAL)r.left+.5f,y=(REAL)r.top+.5f,w=(REAL)(r.right-r.left)-1.f,h=(REAL)(r.bottom-r.top)-1.f,a=(REAL)std::min<LONG>(diameter,std::min(r.right-r.left-1,r.bottom-r.top-1));
    if(a<=0){fill(d,r,bg);return;}
    path.AddArc(x,y,a,a,180,90);path.AddArc(x+w-a,y,a,a,270,90);path.AddArc(x+w-a,y+h-a,a,a,0,90);path.AddArc(x,y+h-a,a,a,90,90);path.CloseFigure();
    SolidBrush brush(color(bg));g.FillPath(&brush,&path);if(outline){Pen pen(color(border),1.f);g.DrawPath(&pen,&path);}
}
static void openTab(HDC d,RECT r,int radius,COLORREF bg,COLORREF border,bool towardTop){
    using namespace Gdiplus;
    Graphics g(d);g.SetSmoothingMode(SmoothingModeAntiAlias);
    if(towardTop){g.TranslateTransform(0.f,(REAL)(r.top+r.bottom));g.ScaleTransform(1.f,-1.f);}
    REAL x=(REAL)r.left+.5f,y=(REAL)r.top+.5f,w=(REAL)(r.right-r.left)-1.f,h=(REAL)(r.bottom-r.top),a=(REAL)std::max(1,std::min(radius,(int)(r.bottom-r.top)/2));
    GraphicsPath path;
    path.AddBezier(x-a,y+h,x,y+h,x,y+h,x,y+h-a);
    path.AddLine(x,y+h-a,x,y+a);path.AddArc(x,y,a*2,a*2,180,90);
    path.AddLine(x+a,y,x+w-a,y);path.AddArc(x+w-a*2,y,a*2,a*2,270,90);
    path.AddLine(x+w,y+a,x+w,y+h-a);path.AddBezier(x+w,y+h-a,x+w,y+h,x+w,y+h,x+w+a,y+h);
    SolidBrush brush(color(bg));g.FillPath(&brush,&path);
    // Fill closes the outline; DrawPath intentionally leaves the content edge open.
    Pen pen(color(border),1.f);g.DrawPath(&pen,&path);
}
void Tabs::paint(HDC target,RECT rc){
    if(hwnd&&compactFrame&&!layingOut)layoutSeam();
    if(seamWindow)InvalidateRect(seamWindow,nullptr,FALSE);
    int width=rc.right-rc.left,height=rc.bottom-rc.top;if(width<=0||height<=0)return;
    HDC d=CreateCompatibleDC(target);HBITMAP bmp=CreateCompatibleBitmap(target,width,height);auto oldBmp=SelectObject(d,bmp);
    bool dark=session&&session->darkMode();bool contrast=false;HIGHCONTRASTW hc{sizeof(hc)};if(SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(hc),&hc,0))contrast=(hc.dwFlags&HCF_HIGHCONTRASTON)!=0;
    bool blended=session&&session->settings.barStyle==1,towardTop=session&&session->settings.position==3;
    COLORREF lightBackground=session?session->settings.barColor:tabcolor::defaultBackground();
    COLORREF bg=tabcolor::background(dark,blended,lightBackground,session&&session->settings.customBarColor);
    if(dark&&blended&&session)bg=session->surroundColor();
    COLORREF fg=dark?RGB(235,235,235):RGB(50,50,50),selected=blended?(dark?RGB(50,50,50):RGB(255,255,255)):(dark?RGB(58,58,58):RGB(237,237,237)),accent=dark?RGB(112,166,242):RGB(35,91,180);
    COLORREF border=dark?RGB(80,80,80):RGB(213,213,213);
    if(session&&session->host==Host::Excel)accent=dark?RGB(91,190,137):RGB(16,124,65);
    if(session&&session->host==Host::PowerPoint)accent=dark?RGB(240,141,107):RGB(196,59,28);
    if(!dark)fg=tabcolor::foreground(bg);
    if(contrast){bg=GetSysColor(COLOR_WINDOW);fg=GetSysColor(COLOR_WINDOWTEXT);selected=GetSysColor(COLOR_HIGHLIGHT);accent=GetSysColor(COLOR_HIGHLIGHTTEXT);border=fg;}
    int scale=dpi(hwnd),pad=MulDiv(8,scale,96),button=MulDiv(30,scale,96),tabH=height;
    RECT local{0,0,width,height};fill(d,local,contrast?bg:(session?session->surroundColor():tabcolor::surround(dark)));SetBkMode(d,TRANSPARENT);
    int panelInset=dark&&blended?0:MulDiv(8,scale,96),panelRadius=dark&&blended?0:MulDiv(16,scale,96);
    int outerY=dark&&blended?0:MulDiv(1,scale,96);RECT panel{panelInset,outerY,width-panelInset,height-outerY};
    if(!blended){
        RECT shadow=panel;shadow.top+=MulDiv(1,scale,96);shadow.bottom+=MulDiv(1,scale,96);
        rounded(d,shadow,panelRadius,dark?RGB(24,24,24):RGB(218,216,214),bg);
        rounded(d,panel,panelRadius,bg,bg);
    }else rounded(d,panel,panelRadius,bg,bg);
    // Both styles share Office's 8 DIP side insets and rounded outer card.
    // Clip open-tab feet to that card so they cannot flatten its corners.
    int savedClip=SaveDC(d);
    if(!contrast&&panelRadius){HRGN clip=CreateRoundRectRgn(panel.left,panel.top,panel.right+1,panel.bottom+1,panelRadius,panelRadius);ExtSelectClipRgn(d,clip,RGN_AND);DeleteObject(clip);}
    if(blended){tabH=panel.bottom;RECT baseline{panel.left,towardTop?panel.top:panel.bottom-1,panel.right,towardTop?panel.top+1:panel.bottom};fill(d,baseline,border);}
    LOGFONTW lf{};lf.lfHeight=-MulDiv(12,scale,96);wcscpy_s(lf.lfFaceName,L"Microsoft YaHei UI");HFONT font=CreateFontIndirectW(&lf);auto oldFont=SelectObject(d,font);
    hit.clear();int nextButton=panelInset+pad;
    int contentLeft=panelInset+pad,contentRight=std::max<int>(contentLeft,panel.right-pad-3*button);
    int available=contentRight-contentLeft;
    if(session){
        auto views=session->tabViews(hostWindow);auto selectedId=session->activeTabId(hostWindow);
        std::vector<int> widths;for(auto& v:views){SIZE textSize{};GetTextExtentPoint32W(d,v->title.c_str(),(int)v->title.size(),&textSize);int preferred=v->manualTabWidth?v->manualTabWidth:tablayout::preferred(session->settings.tabWidth,session->settings.widthMode,MulDiv(textSize.cx,96,scale),MulDiv(available,96,scale),(int)views.size());widths.push_back(MulDiv(preferred,scale,96));}
        offset=std::clamp(offset,0,std::max(0,(int)views.size()-1));
        if(lastSelected!=selectedId){lastSelected=selectedId;for(int i=0;i<(int)views.size();i++)if(views[i]->id==selectedId){if(i<offset)offset=i;int total=0;for(int j=offset;j<=i;j++)total+=widths[j];while(total>available&&offset<i)total-=widths[offset++];break;}}
        int x=contentLeft;
        for(int i=offset;i<(int)views.size()&&x<contentRight;i++){
            int tabW=widths[i];if(contentRight-x<MulDiv(tablayout::minWidth,scale,96))break;
            auto& v=views[i];RECT r{x,MulDiv(3,scale,96),std::min(contentRight,x+tabW-MulDiv(3,scale,96)),tabH-MulDiv(3,scale,96)};
            bool active=v->id==selectedId;
            if(blended&&active){if(towardTop)r.top=panel.top;else r.bottom=tabH;}
            COLORREF tabColor=active?selected:(v->id==hover?(dark?RGB(49,49,49):RGB(238,238,238)):bg);
            if(blended&&active)openTab(d,r,MulDiv(8,scale,96),tabColor,border,towardTop);
            else rounded(d,r,MulDiv(16,scale,96),tabColor,border,active);
            if(active&&session->settings.accentPosition!=2){int thick=std::max(2,MulDiv(2,scale,96));int y=session->settings.accentPosition==1?r.top+MulDiv(1,scale,96):r.bottom-thick-MulDiv(1,scale,96);RECT line{r.left+pad,y,r.right-pad,y+thick};HBRUSH b=CreateSolidBrush(accent);auto ob=SelectObject(d,b);auto op=SelectObject(d,GetStockObject(NULL_PEN));RoundRect(d,line.left,line.top,line.right,line.bottom,thick,thick);SelectObject(d,ob);SelectObject(d,op);DeleteObject(b);}
            RECT label{r.left+pad,r.top,r.right-button,r.bottom-MulDiv(2,scale,96)};std::wstring title=std::wstring(v->locked?L"● ":L"")+(v->saved?L"":L"* ")+v->title;
            COLORREF tabForeground=contrast?(active?GetSysColor(COLOR_HIGHLIGHTTEXT):fg):tabcolor::foreground(tabColor);
            text(d,title,label,tabForeground);
            RECT closer{r.right-button,r.top,r.right-MulDiv(4,scale,96),r.bottom};text(d,v->locked?L"●":L"×",closer,tabForeground,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            POINT cursor{};GetCursorPos(&cursor);ScreenToClient(hwnd,&cursor);
            if(v->id==resizing||(cursor.x>=r.right-MulDiv(4,scale,96)&&cursor.x<=r.right+MulDiv(4,scale,96)&&cursor.y>=r.top&&cursor.y<r.bottom)){RECT grip{r.right-std::max(1,MulDiv(1,scale,96)),r.top+pad,r.right+std::max(1,MulDiv(1,scale,96)),r.bottom-pad};fill(d,grip,accent);}
            hit.push_back({v->id,r});x=std::min(contentRight,x+tabW);
        }
        nextButton=x;
    }
    nextButton=std::min(nextButton,std::max(0,width-3*button));
    addRect={nextButton,0,nextButton+button,tabH-MulDiv(2,scale,96)};listRect={nextButton+button,0,nextButton+2*button,tabH-MulDiv(2,scale,96)};settingsRect={nextButton+2*button,0,nextButton+3*button,tabH-MulDiv(2,scale,96)};
    auto icon=[&](RECT r,int kind){
        POINT cursor{};GetCursorPos(&cursor);ScreenToClient(hwnd,&cursor);
        if(PtInRect(&r,cursor)){RECT hoverRect{r.left+2,r.top+MulDiv(3,scale,96),r.right-2,r.bottom-MulDiv(3,scale,96)};rounded(d,hoverRect,MulDiv(10,scale,96),dark?RGB(65,65,65):RGB(240,240,240),bg);}
        using namespace Gdiplus;Graphics g(d);g.SetSmoothingMode(SmoothingModeAntiAlias);
        REAL cx=(REAL)(r.left+r.right)/2,cy=(REAL)(r.top+r.bottom)/2,arm=(REAL)MulDiv(5,scale,96),stroke=(REAL)scale/96*1.5f;
        Pen pen(color(fg),stroke);pen.SetStartCap(LineCapRound);pen.SetEndCap(LineCapRound);pen.SetLineJoin(LineJoinRound);
        if(kind==0){g.DrawLine(&pen,cx-arm,cy,cx+arm,cy);g.DrawLine(&pen,cx,cy-arm,cx,cy+arm);}
        else if(kind==1){PointF points[]{{cx-arm,cy-arm/2},{cx,cy+arm/2},{cx+arm,cy-arm/2}};g.DrawLines(&pen,points,3);}
        else{SolidBrush brush(color(fg));REAL dot=(REAL)scale/96*2.5f;for(int i=-1;i<=1;i++)g.FillEllipse(&brush,cx+i*MulDiv(5,scale,96)-dot/2,cy-dot/2,dot,dot);}
    };icon(addRect,0);icon(listRect,1);icon(settingsRect,2);
    if(dragging){RECT marker{down.x-1,3,down.x+2,tabH-3};fill(d,marker,accent);}
    SelectObject(d,oldFont);DeleteObject(font);RestoreDC(d,savedClip);BitBlt(target,rc.left,rc.top,width,height,d,0,0,SRCCOPY);SelectObject(d,oldBmp);DeleteObject(bmp);DeleteDC(d);
}
void Tabs::click(UINT msg,WPARAM,LPARAM lp){
    if(!session)return;
    auto s=session->shared_from_this();HWND display=hwnd;
    POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};auto id=at(p);
    if(msg==WM_MOUSEMOVE&&resizing){
        auto v=s->find(resizing);if(v){v->manualTabWidth=tablayout::resized(resizeStartWidth,p.x-down.x,dpi(hwnd));s->redraw();}else finishResize(false);
        SetCursor(LoadCursorW(nullptr,IDC_SIZEWE));return;
    }
    if(msg==WM_LBUTTONDOWN){auto edge=edgeAt(p);if(edge){auto v=s->find(edge);if(v){resizing=edge;resizeOriginalWidth=v->manualTabWidth;for(auto& x:hit)if(x.first==edge)resizeStartWidth=std::clamp(MulDiv(x.second.right-x.second.left+MulDiv(3,dpi(hwnd),96),96,dpi(hwnd)),tablayout::minWidth,tablayout::maxWidth);down=p;pressed=0;dragging=false;resizePreviousFocus=GetFocus();if(hwnd)SetFocus(hwnd);SetCapture(hwnd);SetCursor(LoadCursorW(nullptr,IDC_SIZEWE));}return;}}
    if(msg==WM_LBUTTONUP&&resizing){finishResize(false);if(IsWindow(display))InvalidateRect(display,nullptr,FALSE);return;}
    if(msg==WM_MOUSEMOVE){if(hover!=id){hover=id;InvalidateRect(hwnd,nullptr,FALSE);}
        if(pressed&&GetCapture()==hwnd&&(abs(p.x-down.x)>GetSystemMetrics(SM_CXDRAG)||abs(p.y-down.y)>GetSystemMetrics(SM_CYDRAG))&&!pressedClose){dragging=true;down=p;InvalidateRect(hwnd,nullptr,FALSE);}return;}
    if(msg==WM_LBUTTONDOWN){pressed=id;down=p;pressedClose=false;for(auto& x:hit)if(x.first==id)pressedClose=p.x>=x.second.right-MulDiv(32,dpi(hwnd),96);if(id)SetCapture(hwnd);return;}
    if(msg==WM_LBUTTONUP){auto was=pressed;bool moved=dragging,closeHit=pressedClose;pressed=0;dragging=false;if(GetCapture()==hwnd)ReleaseCapture();
        if(moved&&was){if(id){if(id!=was)s->reorder(was,id);}else{RECT client{};GetClientRect(display,&client);if(!PtInRect(&client,p)){POINT screen=p;ClientToScreen(display,&screen);s->drop(was,screen);}}}
        else if(id&&id==was){if(closeHit)s->close(id);else s->activate(id,hostWindow);}
        else if(!was&&PtInRect(&addRect,p))s->create(false,hostWindow);
        else if(!was&&PtInRect(&settingsRect,p)){ClientToScreen(display,&p);s->options(display,p);}
        else if(!was&&PtInRect(&listRect,p)){HMENU m=CreatePopupMenu();UINT n=1;std::vector<unsigned long long> ids;for(auto& v:s->tabViews(hostWindow)){AppendMenuW(m,MF_STRING|(v->id==s->activeTabId(hostWindow)?MF_CHECKED:0),n++,(v->title+(v->saved?L"":L" *")).c_str());ids.push_back(v->id);}ClientToScreen(display,&p);UINT choice=TrackPopupMenu(m,TPM_RETURNCMD|TPM_NONOTIFY,p.x,p.y,0,display,nullptr);DestroyMenu(m);if(choice&&choice<=ids.size())s->activate(ids[choice-1],hostWindow);}if(IsWindow(display))InvalidateRect(display,nullptr,FALSE);return;}
    if(msg==WM_MBUTTONUP&&id){s->close(id);return;}
    if(msg==WM_LBUTTONDBLCLK){if(edgeAt(p))return;if(id&&s->settings.doubleClose)s->close(id);else if(!id&&!PtInRect(&addRect,p)&&!PtInRect(&listRect,p)&&!PtInRect(&settingsRect,p))s->create(false,hostWindow);return;}
    if(msg==WM_RBUTTONUP){ClientToScreen(display,&p);if(id)s->menu(display,id,p);else s->options(display,p);}
}
LRESULT CALLBACK Tabs::proc(HWND w,UINT m,WPARAM a,LPARAM b){
    auto self=(Tabs*)GetWindowLongPtrW(w,GWLP_USERDATA);
    if(m==WM_NCCREATE){self=(Tabs*)((CREATESTRUCTW*)b)->lpCreateParams;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)self);}
    if(!self)return DefWindowProcW(w,m,a,b);
    Ptr<Tabs> alive(self);
    try{
        if(m==WM_PAINT){PAINTSTRUCT ps;HDC d=BeginPaint(w,&ps);if(!self->hwnd||w==self->hwnd){RECT rc;GetClientRect(w,&rc);self->paint(d,rc);}EndPaint(w,&ps);return 0;}
        if(m==WM_ERASEBKGND)return 1;
        if(m==WM_SIZE||m==WM_DPICHANGED_AFTERPARENT){InvalidateRect(w,nullptr,FALSE);return 0;}
        if(m==WM_MOUSEWHEEL){self->offset=std::max(0,self->offset+(GET_WHEEL_DELTA_WPARAM(a)>0?-1:1));InvalidateRect(w,nullptr,FALSE);return 0;}
        if(m==WM_MOUSEMOVE||m==WM_LBUTTONDOWN||m==WM_LBUTTONUP||m==WM_MBUTTONUP||m==WM_RBUTTONUP||m==WM_LBUTTONDBLCLK){self->click(m,a,b);return 0;}
        if(m==WM_KEYDOWN){MSG msg{w,m,a,b};if(self->TranslateAccelerator(&msg)==S_OK)return 0;}
        if(m==WM_SETCURSOR&&LOWORD(b)==HTCLIENT){POINT p{};GetCursorPos(&p);ScreenToClient(w,&p);if(self->resizing||self->edgeAt(p)){SetCursor(LoadCursorW(nullptr,IDC_SIZEWE));return TRUE;}}
        if(m==WM_CAPTURECHANGED){self->finishResize(true);self->pressed=0;self->dragging=false;}
        if(m==WM_CANCELMODE){self->finishResize(true);self->pressed=0;self->dragging=false;if(GetCapture()==w)ReleaseCapture();return 0;}
        if(m==WM_NOTIFY&&((NMHDR*)b)->code==TTN_GETDISPINFOW){auto info=(NMTTDISPINFOW*)b;POINT p;GetCursorPos(&p);ScreenToClient(w,&p);auto v=self->session?self->session->find(self->at(p)):nullptr;self->hint=v?(v->path.empty()?v->title:v->path):i18n::text(self->session?self->session->language():0,L"openOfficetab · + 新建 · ▾ 文档列表 · ⋯ 设置\n拖动排序，中键关闭，右键更多操作");info->lpszText=self->hint.data();return 0;}
        if(m==WM_NCDESTROY){if(self->hwnd==w)self->hwnd=nullptr;if(self->oleHwnd==w)self->oleHwnd=nullptr;SetWindowLongPtrW(w,GWLP_USERDATA,0);}
    }catch(const ComError& e){if(self->session)self->session->report(e);}catch(...){log(L"control.exception",E_FAIL);}
    return DefWindowProcW(w,m,a,b);
}
