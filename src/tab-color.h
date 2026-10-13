#pragma once
#include "com.h"
namespace tabcolor {
inline COLORREF defaultBackground(){return RGB(240,240,240);}
inline bool parse(const std::wstring& value,COLORREF& color){
    size_t start=!value.empty()&&value[0]==L'#'?1:0;if(value.size()!=start+6)return false;
    unsigned rgb=0;for(size_t i=start;i<value.size();i++){wchar_t c=value[i];unsigned digit;
        if(c>=L'0'&&c<=L'9')digit=c-L'0';else if(c>=L'A'&&c<=L'F')digit=c-L'A'+10;else if(c>=L'a'&&c<=L'f')digit=c-L'a'+10;else return false;rgb=(rgb<<4)|digit;}
    color=RGB((rgb>>16)&255,(rgb>>8)&255,rgb&255);return true;
}
inline std::wstring format(COLORREF color){wchar_t text[8];swprintf_s(text,L"#%02X%02X%02X",GetRValue(color),GetGValue(color),GetBValue(color));return text;}
inline COLORREF foreground(COLORREF bg){return (299*GetRValue(bg)+587*GetGValue(bg)+114*GetBValue(bg))<140000?RGB(245,245,245):RGB(50,50,50);}
}
