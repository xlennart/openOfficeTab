#pragma once
#include "com.h"
#include <set>
namespace storage {
using Section=std::map<std::wstring,std::wstring>;
using Ini=std::map<std::wstring,Section>;
struct FilePreference {int width=0;bool independent=false;};
// Explicit roots keep native regression tests away from the user's profile.
class Store {
    std::wstring directory;
public:
    explicit Store(std::wstring root):directory(std::move(root)){}
    std::wstring path(const wchar_t* file)const{return directory+L"\\"+file;}
    Ini read(const wchar_t* file,bool* success=nullptr)const;
    bool update(const wchar_t* file,const std::function<void(Ini&)>& mutate)const;
    unsigned long long stamp(const wchar_t* file)const;
    bool preference(const std::wstring& host,const std::wstring& file,FilePreference& result)const;
    bool savePreference(const std::wstring& host,const std::wstring& file,const FilePreference& value)const;
    bool resetPreference(const std::wstring& host,const std::wstring& file=L"")const;
    std::vector<std::wstring> recent(const std::wstring& host)const;
    bool closed(const std::wstring& host,const std::wstring& file)const;
    bool clearRecent(const std::wstring& host)const;
};
std::wstring normalize(const std::wstring& file);
bool localFile(const std::wstring& file);
std::wstring encode(const std::wstring& value);
std::wstring decode(const std::wstring& value);
int number(const Section& values,const wchar_t* key,int fallback);
std::wstring value(const Section& values,const wchar_t* key,const std::wstring& fallback=L"");
}
