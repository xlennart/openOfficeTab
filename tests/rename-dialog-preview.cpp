// Hidden native windows only. Render the real dialog; never starts Office.
#include "../src/rename-dialog.h"
#include <gdiplus.h>
#include <cassert>
#include <iostream>
class PreviewApplication final:public IDispatch {
    ULONG refs=1;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IDispatch)return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    DISPATCH_STUBS
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID* ids)override{*ids=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT* result,EXCEPINFO*,UINT*)override{if(result){VariantInit(result);result->vt=VT_BSTR;result->bstrVal=SysAllocString(L"Word");}return S_OK;}
};
void capture(HWND window,const wchar_t* path){
    RECT r{};GetClientRect(window,&r);HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,r.right,r.bottom);auto old=SelectObject(dc,bitmap);
    SendMessageW(window,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT);
    for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
        RECT bounds{};GetWindowRect(child,&bounds);MapWindowPoints(nullptr,window,(POINT*)&bounds,2);
        int saved=SaveDC(dc);SetViewportOrgEx(dc,bounds.left,bounds.top,nullptr);
        IntersectClipRect(dc,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top);
        SendMessageW(child,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND);RestoreDC(dc,saved);
    }
    Gdiplus::Bitmap image(bitmap,nullptr);CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
    assert(image.Save(path,&png,nullptr)==Gdiplus::Ok);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);
}
int wmain(){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);module=GetModuleHandleW(nullptr);
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;Gdiplus::GdiplusStartup(&token,&input,nullptr);
    {
        Ptr<IDispatch> app(new PreviewApplication(),false);
        auto session=std::make_shared<Session>(app.get(),L"artifacts\\rename-dialog-preview-state");
        struct Template{DLGTEMPLATE dialog;WORD menu,cls,title;} data{};
        data.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN|DS_MODALFRAME;data.dialog.cx=290;data.dialog.cy=125;
        for(int mode=0;mode<3;mode++){
            session->host=mode==1?Host::Excel:Host::Word;session->settings.themeMode=mode==1?2:1;
            renameui::RenameChoice choice{session.get(),1,mode==2?L"C:\\Examples\\Quarterly report.docx":L"C:\\Examples\\项目进度报告.docx",mode==2?2:1};
            HWND dialog=CreateDialogIndirectParamW(module,&data.dialog,nullptr,renameui::renameProc,(LPARAM)&choice);
            assert(dialog&&!IsWindowVisible(dialog));
            // EDIT paints text only when its hierarchy is visible. Keep the entire
            // window outside every monitor and never activate it or take focus.
            SetWindowLongPtrW(dialog,GWL_EXSTYLE,GetWindowLongPtrW(dialog,GWL_EXSTYLE)|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW);
            SetWindowPos(dialog,nullptr,-30000,-30000,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
            ShowWindow(dialog,SW_SHOWNOACTIVATE);UpdateWindow(dialog);
            assert((GetWindowLongW(GetDlgItem(dialog,IDOK),GWL_STYLE)&BS_TYPEMASK)==BS_OWNERDRAW);
            assert(SendDlgItemMessageW(dialog,101,EM_GETLIMITTEXT,0,0)==255);
            HDC fontDc=GetDC(dialog);auto oldFont=SelectObject(fontDc,choice.title);wchar_t face[LF_FACESIZE]{};GetTextFaceW(fontDc,LF_FACESIZE,face);std::wcout<<L"Title font: "<<face<<L"\n";SelectObject(fontDc,oldFont);ReleaseDC(dialog,fontDc);
            for(int id:{110,111,112,101,113,114,IDCANCEL,IDOK}){
                RECT child{},client{};GetWindowRect(GetDlgItem(dialog,id),&child);MapWindowPoints(nullptr,dialog,(POINT*)&child,2);GetClientRect(dialog,&client);
                assert(child.left>=0&&child.top>=0&&child.right<=client.right&&child.bottom<=client.bottom);
            }
            capture(dialog,mode==0?L"artifacts/rename-dialog-light.png":mode==1?L"artifacts/rename-dialog-dark.png":L"artifacts/rename-dialog-en.png");
            DestroyWindow(dialog);
        }
    }
    Gdiplus::GdiplusShutdown(token);CoUninitialize();
    std::cout<<"Hidden dialog renders: light/dark, Chinese/English, bounds, owner-drawn default and edit limit passed.\n";
}
