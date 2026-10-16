#include "model.h"
#include "theme.h"
#include "tab-color.h"
bool Session::darkMode()const{return officetheme::dark(settings.themeMode,officeTheme,systemDark);}
COLORREF Session::surroundColor()const{
    if(darkMode()&&settings.themeMode==0&&officeTheme==3)return RGB(68,68,68);
    return tabcolor::surround(darkMode());
}
bool Session::pollTheme(bool force){
    auto now=GetTickCount64();if(!force&&now-lastThemePoll<500)return false;lastThemePoll=now;
    auto previous=darkMode();auto surface=surroundColor();officeTheme=officetheme::office();systemDark=officetheme::windowsDark();
    bool changed=previous!=darkMode()||surface!=surroundColor();if(changed)redraw();return changed;
}
bool Session::setThemeMode(int mode){if(stopped||mode<0||mode>3)return false;settings.themeMode=mode;settings.dark=mode==2;pollTheme(true);settings.save();redraw();return true;}
bool Session::setFrameCloseAction(int action){if(stopped||action<0||action>2)return false;settings.frameCloseAction=action;return settings.save();}
