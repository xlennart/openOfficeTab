#include "storage.h"
#include "layout.h"
#include <cwctype>
#include <limits>
#include <cstring>

namespace storage {
namespace {
constexpr DWORD maxFile=4*1024*1024;
std::wstring hash(const std::wstring& text){unsigned long long n=14695981039346656037ull;for(wchar_t c:text){n^=(unsigned short)c;n*=1099511628211ull;}wchar_t buffer[24];swprintf_s(buffer,L"%016llx",n);return buffer;}
std::wstring section(const std::wstring& host,const std::wstring& file){return L"file."+host+L"."+hash(normalize(file));}
bool write(const std::wstring& file,const Ini& ini){
    std::wstring content;for(auto& s:ini){content+=L"["+s.first+L"]\r\n";for(auto& v:s.second)content+=v.first+L"="+v.second+L"\r\n";}
    static std::atomic<unsigned long> sequence{0};auto temp=file+L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(++sequence)+L".tmp";
    HANDLE h=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;WORD bom=0xfeff;bool ok=WriteFile(h,&bom,2,&written,nullptr)&&written==2;
    DWORD size=(DWORD)(content.size()*sizeof(wchar_t));ok=ok&&WriteFile(h,content.data(),size,&written,nullptr)&&written==size&&FlushFileBuffers(h);CloseHandle(h);
    if(ok)ok=MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!ok)DeleteFileW(temp.c_str());return ok;
}
}
std::wstring normalize(const std::wstring& file){
    if(file.empty())return L"";std::wstring result=file;
    if(result.find(L"://")==std::wstring::npos){DWORD n=GetFullPathNameW(file.c_str(),0,nullptr,nullptr);if(n&&n<32768){std::vector<wchar_t> buffer(n);if(GetFullPathNameW(file.c_str(),n,buffer.data(),nullptr))result=buffer.data();}}
    std::replace(result.begin(),result.end(),L'/',L'\\');if(!result.empty())CharLowerBuffW(result.data(),(DWORD)result.size());return result;
}
bool localFile(const std::wstring& file){
    if(file.size()<3||file.find(L"://")!=std::wstring::npos)return false;
    bool absolute=(file[1]==L':'&&(file[2]==L'\\'||file[2]==L'/'))||(file[0]==L'\\'&&file[1]==L'\\');
    return absolute&&file.find(L'\r')==std::wstring::npos&&file.find(L'\n')==std::wstring::npos;
}
std::wstring encode(const std::wstring& value){std::wstring out;out.reserve(value.size()*4);for(wchar_t c:value){wchar_t buffer[5];swprintf_s(buffer,L"%04X",(unsigned short)c);out+=buffer;}return out;}
std::wstring decode(const std::wstring& value){
    if(value.size()%4||value.size()>32768*4)return L"";std::wstring out;
    for(size_t i=0;i<value.size();i+=4){unsigned word=0;for(size_t j=0;j<4;j++){wchar_t c=value[i+j];unsigned digit;if(c>=L'0'&&c<=L'9')digit=c-L'0';else if(c>=L'A'&&c<=L'F')digit=c-L'A'+10;else if(c>=L'a'&&c<=L'f')digit=c-L'a'+10;else return L"";word=(word<<4)|digit;}if(!word)return L"";out+=(wchar_t)word;}return out;
}
int number(const Section& values,const wchar_t* key,int fallback){auto text=value(values,key);if(text.empty())return fallback;wchar_t* end=nullptr;errno=0;long n=wcstol(text.c_str(),&end,10);return errno||*end||n<std::numeric_limits<int>::min()||n>std::numeric_limits<int>::max()?fallback:(int)n;}
std::wstring value(const Section& values,const wchar_t* key,const std::wstring& fallback){auto it=values.find(key);return it==values.end()?fallback:it->second;}
Ini Store::read(const wchar_t* file,bool* success)const{
    if(success)*success=false;
    Ini ini;HANDLE h=CreateFileW(path(file).c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE){DWORD error=GetLastError();if(success&&(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND))*success=true;return ini;}
    LARGE_INTEGER length{};if(!GetFileSizeEx(h,&length)||length.QuadPart<0||length.QuadPart>maxFile){CloseHandle(h);return ini;}
    std::vector<char> data((size_t)length.QuadPart);DWORD read=0;bool ok=ReadFile(h,data.data(),(DWORD)data.size(),&read,nullptr)&&read==data.size();CloseHandle(h);if(!ok)return ini;if(data.empty()){if(success)*success=true;return ini;}
    std::wstring text;if(data.size()>=2&&(unsigned char)data[0]==255&&(unsigned char)data[1]==254){if(data.size()%2)return ini;text.resize((data.size()-2)/2);if(!text.empty())std::memcpy(text.data(),data.data()+2,data.size()-2);}
    else{int skip=data.size()>=3&&(unsigned char)data[0]==239&&(unsigned char)data[1]==187&&(unsigned char)data[2]==191?3:0;int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,data.data()+skip,(int)data.size()-skip,nullptr,0);if(!count)return ini;text.resize(count);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,data.data()+skip,(int)data.size()-skip,text.data(),count);}
    std::wistringstream input(text);std::wstring line,current;while(std::getline(input,line)){if(!line.empty()&&line.back()==L'\r')line.pop_back();if(line.empty()||line[0]==L';'||line[0]==L'#')continue;
        if(line.front()==L'['&&line.back()==L']'){current=line.substr(1,line.size()-2);continue;}auto equal=line.find(L'=');if(!current.empty()&&equal!=std::wstring::npos)ini[current][line.substr(0,equal)]=line.substr(equal+1);}
    if(success)*success=true;return ini;
}
bool Store::update(const wchar_t* file,const std::function<void(Ini&)>& mutate)const{
    auto name=L"Local\\openOfficetab.State."+hash(normalize(directory));HANDLE mutex=CreateMutexW(nullptr,FALSE,name.c_str());if(!mutex)return false;
    DWORD wait=WaitForSingleObject(mutex,0);if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){CloseHandle(mutex);return false;}
    bool ok=false;try{CreateDirectoryW(directory.c_str(),nullptr);bool readable=false;auto ini=read(file,&readable);
        if(readable&&(!wcscmp(file,L"documents.ini")?number(ini[L"metadata"],L"schemaVersion",1)==1:true)){
            mutate(ini);if(!wcscmp(file,L"documents.ini"))ini[L"metadata"][L"schemaVersion"]=L"1";ok=write(path(file),ini);}
    }catch(...){ReleaseMutex(mutex);CloseHandle(mutex);throw;}
    ReleaseMutex(mutex);CloseHandle(mutex);return ok;
}
unsigned long long Store::stamp(const wchar_t* file)const{WIN32_FILE_ATTRIBUTE_DATA data{};if(!GetFileAttributesExW(path(file).c_str(),GetFileExInfoStandard,&data))return 0;ULARGE_INTEGER value{};value.LowPart=data.ftLastWriteTime.dwLowDateTime;value.HighPart=data.ftLastWriteTime.dwHighDateTime;return value.QuadPart;}
bool Store::preference(const std::wstring& host,const std::wstring& file,FilePreference& result)const{
    if(!localFile(file))return false;auto ini=read(L"documents.ini");auto it=ini.find(section(host,file));if(it==ini.end()||normalize(decode(value(it->second,L"path")))!=normalize(file))return false;
    result.width=number(it->second,L"width",0);if(result.width!=0&&!tablayout::validWidth(result.width,0))result.width=0;result.independent=number(it->second,L"independent",0)!=0;return true;
}
bool Store::savePreference(const std::wstring& host,const std::wstring& file,const FilePreference& value)const{
    if(!localFile(file)||(value.width!=0&&!tablayout::validWidth(value.width,0)))return false;
    return update(L"documents.ini",[&](Ini& ini){ini[section(host,file)]={{L"path",encode(file)},{L"width",std::to_wstring(value.width)},{L"independent",value.independent?L"1":L"0"}};});
}
bool Store::resetPreference(const std::wstring& host,const std::wstring& file)const{return update(L"documents.ini",[&](Ini& ini){if(!file.empty())ini.erase(section(host,file));else for(auto it=ini.begin();it!=ini.end();)if(it->first.rfind(L"file."+host+L".",0)==0)it=ini.erase(it);else ++it;});}
std::vector<std::wstring> Store::recent(const std::wstring& host)const{
    auto ini=read(L"documents.ini");auto& values=ini[L"recent."+host];std::vector<std::wstring> result;
    for(int i=0;i<20;i++){auto file=decode(value(values,(L"path"+std::to_wstring(i)).c_str()));if(localFile(file))result.push_back(file);}return result;
}
bool Store::closed(const std::wstring& host,const std::wstring& file)const{
    if(!localFile(file))return false;
    return update(L"documents.ini",[&](Ini& ini){auto& values=ini[L"recent."+host];std::vector<std::wstring> files{file};std::set<std::wstring> seen{normalize(file)};
        for(int i=0;i<20&&files.size()<20;i++){auto old=decode(value(values,(L"path"+std::to_wstring(i)).c_str()));if(localFile(old)&&seen.insert(normalize(old)).second)files.push_back(old);}
        values.clear();for(size_t i=0;i<files.size();i++)values[L"path"+std::to_wstring(i)]=encode(files[i]);});
}
bool Store::clearRecent(const std::wstring& host)const{return update(L"documents.ini",[&](Ini& ini){ini.erase(L"recent."+host);});}
}
