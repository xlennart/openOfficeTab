#pragma once
#include "com.h"
namespace officetheme {
// Office's explicit theme wins in Auto; Office "Use system setting" (6),
// absent or unknown values follow the Windows application color preference.
inline bool dark(int mode,int office,bool windowsDark){
    if(mode==1)return false;if(mode==2)return true;if(mode==3)return windowsDark;
    if(office==3||office==4)return true;if(office==0||office==5)return false;return windowsDark;
}
inline DWORD read(const wchar_t* key,const wchar_t* name,DWORD fallback){DWORD value=0,size=sizeof(value);return RegGetValueW(HKEY_CURRENT_USER,key,name,RRF_RT_REG_DWORD,nullptr,&value,&size)==ERROR_SUCCESS?value:fallback;}
inline int office(){return (int)read(L"Software\\Microsoft\\Office\\16.0\\Common",L"UI Theme",(DWORD)-1);}
inline bool windowsDark(){return read(L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",1)==0;}
}
