#pragma once
#include <string>
#include <climits>

// Pure filename policy and save/verify/remove ordering, testable without Office.
namespace documentrename {
inline std::wstring leaf(const std::wstring& path){return path.substr(path.find_last_of(L"\\/")+1);}
inline std::wstring extension(const std::wstring& path){
    auto name=leaf(path);auto dot=name.find_last_of(L'.');
    return dot==std::wstring::npos||dot==0?L"":name.substr(dot);
}
inline std::wstring stem(const std::wstring& path){auto name=leaf(path);return name.substr(0,name.size()-extension(path).size());}
inline bool validStem(const std::wstring& name,const std::wstring& suffix){
    if(name.empty()||name==L"."||name==L".."||name.back()==L'.'||name.back()==L' ')return false;
    auto full=name+suffix;size_t units=0;
    for(auto c:full){
        if(c<32||std::wstring(L"<>:\"/\\|?*").find(c)!=std::wstring::npos)return false;
#if WCHAR_MAX > 0xffff
        units+=c>0xffff?2:1;
#else
        ++units; // Windows wchar_t is already one UTF-16 code unit.
#endif
    }
    if(units>255)return false;
    auto base=full.substr(0,full.find(L'.'));while(!base.empty()&&base.back()==L' ')base.pop_back();
    for(auto& c:base)if(c>=L'a'&&c<=L'z')c-=L'a'-L'A';
    if(base==L"CON"||base==L"PRN"||base==L"AUX"||base==L"NUL"||base==L"CONIN$"||base==L"CONOUT$")return false;
    if(base.size()==4&&(base.substr(0,3)==L"COM"||base.substr(0,3)==L"LPT")&&
        ((base[3]>=L'1'&&base[3]<=L'9')||base[3]==L'\u00b9'||base[3]==L'\u00b2'||base[3]==L'\u00b3'))return false;
    return true;
}
inline std::wstring target(const std::wstring& path,const std::wstring& name){
    auto slash=path.find_last_of(L"\\/");return slash==std::wstring::npos?L"":path.substr(0,slash+1)+name+extension(path);
}
enum class Result {failed,renamed,originalRetained};
template<class Save,class Verify,class Remove>
Result perform(Save&& save,Verify&& verify,Remove&& remove){
    // A canceled, rejected or unverifiable save must never delete the original.
    if(!save()||!verify())return Result::failed;
    return remove()?Result::renamed:Result::originalRetained;
}
}
