#include "../src/tabs.h"
#include "../src/layout.h"
#include "../src/tab-color.h"
#include <cassert>
#include <iostream>

// No Office automation or visible windows: exercise the production GDI renderer
// and pointer handling against two synthetic views.
class FakeApplication final:public IDispatch {
    ULONG refs=1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IDispatch)return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    DISPATCH_STUBS
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID* ids)override{*ids=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT* result,EXCEPINFO*,UINT*)override{if(result){VariantInit(result);result->vt=VT_BSTR;result->bstrVal=SysAllocString(L"Excel");}return S_OK;}
};
int main(){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    Ptr<IDispatch> app(new FakeApplication(),false);
    auto session=std::make_shared<Session>(app.get());
    session->settings=Settings{};session->settings.widthMode=0;
    for(unsigned long long id=1;id<=2;id++){auto v=std::make_shared<View>();v->id=id;v->title=L"Example";session->views.push_back(v);}
    session->activeId=1;session->groupActives[1]=1;
    Ptr<Tabs> tabs(new Tabs(),false);tabs->Bind(session.get(),nullptr);
    HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=720;info.bmiHeader.biHeight=-40;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;HBITMAP image=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto old=SelectObject(dc,image);
    RECT bounds{0,0,720,40};auto paint=[&]{tabs->paint(dc,bounds);GdiFlush();};
    auto countAccent=[&](int top,int bottom,COLORREF expected=RGB(16,124,65)){int count=0;for(int y=top;y<bottom;y++)for(int x=24;x<100;x++)if(GetPixel(dc,x,y)==expected)count++;return count;};
    for(int style=0;style<=1;style++)for(int accent=0;accent<=2;accent++){
        session->settings.barStyle=style;session->settings.accentPosition=accent;paint();
        assert(GetPixel(dc,500,20)==(style?RGB(240,240,240):RGB(255,255,255)));
        assert(accent==2 ? countAccent(0,40)==0 : countAccent(accent==1?0:30,accent==1?10:40)>50);
        if(style){assert(GetPixel(dc,60,37)==RGB(255,255,255));assert(GetPixel(dc,500,38)==RGB(213,213,213));}
        assert(GetPixel(dc,0,20)==RGB(237,235,233));
        assert(GetPixel(dc,8,1)==RGB(237,235,233));
        assert(GetPixel(dc,711,1)==RGB(237,235,233));
        assert(GetPixel(dc,500,3)==(style?RGB(240,240,240):RGB(255,255,255)));
    }
    COLORREF custom=0;assert(tabcolor::parse(L"#F0F0F0",custom)&&custom==RGB(240,240,240));
    assert(tabcolor::parse(L"12aBcD",custom)&&custom==RGB(18,171,205)&&tabcolor::format(custom)==L"#12ABCD");
    for(auto invalid:{L"",L"#FFF",L"#0000000",L"FFGG00",L"#FFFFFF ",L"0x123456"})assert(!tabcolor::parse(invalid,custom));
    assert(tabcolor::foreground(RGB(0,0,0))==RGB(245,245,245)&&tabcolor::foreground(RGB(255,255,255))==RGB(50,50,50));
    session->settings.barColor=RGB(18,171,205);session->settings.customBarColor=true;
    for(int style=0;style<=1;style++){session->settings.barStyle=style;paint();assert(GetPixel(dc,500,20)==RGB(18,171,205));}
    session->settings.barColor=tabcolor::defaultBackground();session->settings.customBarColor=false;session->settings.barStyle=1;
    for(auto host:{Host::Word,Host::Excel,Host::PowerPoint}){
        session->host=host;session->settings.accentPosition=1;paint();
        COLORREF expected=host==Host::Word?RGB(35,91,180):host==Host::Excel?RGB(16,124,65):RGB(196,59,28);
        assert(countAccent(0,10,expected)>50);
    }
    session->host=Host::Excel;session->settings.dark=true;session->settings.themeMode=2;
    for(auto host:{Host::Word,Host::Excel,Host::PowerPoint})for(int position:{1,3}){
        session->host=host;session->settings.position=position;paint();
        assert(GetPixel(dc,500,20)==RGB(38,38,38));
        assert(GetPixel(dc,0,20)==GetPixel(dc,500,20)); // rounded gutters share the surface
        assert(GetPixel(dc,500,position==3?2:0)==GetPixel(dc,500,20)); // no contrasting top strip
        assert(GetPixel(dc,80,25)==RGB(50,50,50)); // selected tab remains distinguishable
    }
    session->settings.dark=false;session->settings.themeMode=1;
    session->settings.position=3;session->settings.barStyle=1;session->settings.accentPosition=2;paint();
    assert(GetPixel(dc,60,2)==RGB(255,255,255));assert(GetPixel(dc,500,1)==RGB(213,213,213));
    session->settings.position=1;paint();
    POINT edge{132,20};assert(tabs->edgeAt(edge)==1);assert(tabs->edgeAt(POINT{110,20})==0);
    auto mouse=[](int x,int y){return MAKELPARAM(x,y);};
    tabs->click(WM_LBUTTONDOWN,0,mouse(132,20));tabs->click(WM_MOUSEMOVE,0,mouse(192,20));tabs->click(WM_LBUTTONUP,0,mouse(192,20));
    assert(session->views[0]->manualTabWidth==180&&session->views[1]->manualTabWidth==0);
    paint();assert(tabs->edgeAt(POINT{192,20})==1);
    tabs->click(WM_LBUTTONDOWN,0,mouse(192,20));tabs->click(WM_MOUSEMOVE,0,mouse(222,20));tabs->finishResize(true);
    assert(session->views[0]->manualTabWidth==180);
    paint();tabs->click(WM_LBUTTONDOWN,0,mouse(192,20));tabs->click(WM_MOUSEMOVE,0,mouse(3000,20));tabs->click(WM_LBUTTONUP,0,mouse(3000,20));
    assert(session->views[0]->manualTabWidth==360&&session->views[1]->manualTabWidth==0);
    assert(!session->setViewWidth(1,79)&&!session->setViewWidth(1,361)&&!session->setViewWidth(99,120));
    assert(session->setViewWidth(1,0));
    tabs->Bind(nullptr,nullptr);tabs.reset();session.reset();app.reset();SelectObject(dc,old);DeleteObject(image);DeleteDC(dc);CoUninitialize();
    std::cout<<"Six appearance combinations, open edges, host color and individual pointer resizing passed.\n";
}
