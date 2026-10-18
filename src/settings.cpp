#include "model.h"
#include "storage.h"
#include "layout.h"
#include "tab-color.h"

namespace {
storage::Section values(const Settings& s){
    return {{L"schemaVersion",L"1"},{L"style",s.barStyle==1?L"open":L"browser"},
        {L"visible",std::to_wstring(s.visible)},{L"group",std::to_wstring(s.group)},{L"dark",std::to_wstring(s.dark)},
        {L"themeMode",std::to_wstring(s.themeMode)},{L"frameCloseAction",std::to_wstring(s.frameCloseAction)},{L"language",std::to_wstring(s.language)},
        {L"autoHide",std::to_wstring(s.autoHide)},{L"doubleClose",std::to_wstring(s.doubleClose)},
        {L"position",std::to_wstring(s.position)},{L"tabWidth",std::to_wstring(s.tabWidth)},{L"widthMode",std::to_wstring(s.widthMode)},
        {L"accentPosition",std::to_wstring(s.accentPosition)},{L"barStyle",std::to_wstring(s.barStyle)},
        {L"barColor",tabcolor::format(s.barColor)},{L"customBarColor",std::to_wstring(s.customBarColor)}};
}
}
bool Settings::load(){
    storage::Store store(root.empty()?dataDir():root);bool readable=false;auto ini=store.read(L"settings.ini",&readable);if(!readable)return false;auto& s=ini[L"openOfficetab"];
    if(storage::number(s,L"schemaVersion",1)!=1){log(L"settings.schema.unsupported");return false;}
    visible=storage::number(s,L"visible",1)!=0;group=storage::number(s,L"group",1)!=0;dark=storage::number(s,L"dark",0)!=0;
    themeMode=storage::number(s,L"themeMode",0);if(themeMode<0||themeMode>3)themeMode=0;
    frameCloseAction=storage::number(s,L"frameCloseAction",0);if(frameCloseAction<0||frameCloseAction>2)frameCloseAction=0;
    language=storage::number(s,L"language",0);if(!i18n::valid(language))language=0;
    autoHide=storage::number(s,L"autoHide",0)!=0;doubleClose=storage::number(s,L"doubleClose",0)!=0;
    position=storage::number(s,L"position",1);if(position!=1&&position!=3)position=1;
    tabWidth=storage::number(s,L"tabWidth",120);widthMode=storage::number(s,L"widthMode",2);if(!tablayout::validWidth(tabWidth,widthMode)){tabWidth=120;widthMode=2;}
    accentPosition=storage::number(s,L"accentPosition",1);barStyle=storage::number(s,L"barStyle",1);if(!tablayout::validAppearance(accentPosition,barStyle)){accentPosition=1;barStyle=1;}
    customBarColor=storage::number(s,L"customBarColor",0)!=0;
    if(!tabcolor::parse(storage::value(s,L"barColor",L"#F0F0F0"),barColor)){barColor=tabcolor::defaultBackground();customBarColor=false;}
    baseline=values(*this);savePending=false;return true;
}
bool Settings::save()const{
    auto current=values(*this);storage::Section delta;
    for(auto& entry:current){auto old=baseline.find(entry.first);if(old==baseline.end()||old->second!=entry.second)delta.insert(entry);}
    if(delta.empty()){savePending=false;return true;}
    storage::Store store(root.empty()?dataDir():root);
    bool ok=store.update(L"settings.ini",[&](storage::Ini& ini){auto& section=ini[L"openOfficetab"];if(storage::number(section,L"schemaVersion",1)!=1)throw ComError(E_FAIL,L"SettingsSchema");for(auto& entry:delta)section[entry.first]=entry.second;section[L"schemaVersion"]=L"1";});
    if(ok)baseline=std::move(current);else log(L"settings.write.pending");savePending=!ok;return ok;
}
bool Settings::synchronize(){
    if(savePending&&!save())return false;
    Settings fresh;fresh.root=root;if(!fresh.load())return false;
    bool different=language!=fresh.language||themeMode!=fresh.themeMode||frameCloseAction!=fresh.frameCloseAction||group!=fresh.group||dark!=fresh.dark||tabWidth!=fresh.tabWidth||widthMode!=fresh.widthMode||accentPosition!=fresh.accentPosition||barStyle!=fresh.barStyle||barColor!=fresh.barColor||customBarColor!=fresh.customBarColor;
    themeMode=fresh.themeMode;frameCloseAction=fresh.frameCloseAction;group=fresh.group;dark=fresh.dark;tabWidth=fresh.tabWidth;widthMode=fresh.widthMode;accentPosition=fresh.accentPosition;barStyle=fresh.barStyle;barColor=fresh.barColor;customBarColor=fresh.customBarColor;
    language=fresh.language;
    for(auto key:{L"language",L"themeMode",L"frameCloseAction",L"group",L"dark",L"tabWidth",L"widthMode",L"accentPosition",L"barStyle",L"style",L"barColor",L"customBarColor"})baseline[key]=fresh.baseline[key];
    return different;
}
