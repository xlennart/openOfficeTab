#pragma once
#include <deque>
#include <initializer_list>

// Only event codes and numeric, process-local identities enter this recorder.
// Never pass document names, paths, Office text or exception descriptions here.
namespace runtimelog {
struct Number {const char* key;long long value;};
struct TraceContext {long long id=0,parent=0;int origin=0;long long job=0;};
alignas(8) inline volatile LONG64 traceCounter=0;
inline thread_local TraceContext traceContext;
inline std::pair<long,long> officeBuild(const std::wstring& text,long numeric=-1){
    std::vector<long> parts;std::wistringstream input(text);std::wstring part;
    while(std::getline(input,part,L'.')){
        if(part.empty()||part.size()>9||!std::all_of(part.begin(),part.end(),[](wchar_t c){return c>=L'0'&&c<=L'9';}))return {-1,-1};
        parts.push_back(wcstol(part.c_str(),nullptr,10));if(parts.size()>4)return {-1,-1};
    }
    if(parts.size()>=3)return {parts[2],parts.size()==4?parts[3]:-1};
    if(parts.size()==1)return {parts[0],-1};
    return {numeric,-1};
}
class Recorder {
    SRWLOCK lock=SRWLOCK_INIT;
    HANDLE file=INVALID_HANDLE_VALUE;
    std::wstring directory,path;
    std::string pending;
    std::deque<std::string> recent;
    size_t recentBytes=0,fileBytes=0;
    unsigned long long sequence=0;
    volatile LONG dropped=0,reportedDropped=0;
    const ULONGLONG runId=GetTickCount64();
    int host=-1;
    static constexpr size_t Limit=1024*1024,Buffer=64*1024;
    static std::string code(const wchar_t* value){
        std::string result;for(size_t i=0;value&&value[i]&&i<160;i++){
            wchar_t c=value[i];result+=((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')||c==L'.'||c==L'_'||c==L'-')?(char)c:'_';
        }return result;
    }
    void prune(){
        WIN32_FIND_DATAW data;HANDLE search=FindFirstFileW((directory+L"\\native-*.jsonl").c_str(),&data);if(search==INVALID_HANDLE_VALUE)return;
        struct Old {std::wstring path;ULONGLONG stamp;DWORD bytes;};std::vector<Old> files;ULONGLONG total=0;
        do{if(data.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;
            ULARGE_INTEGER stamp;stamp.LowPart=data.ftLastWriteTime.dwLowDateTime;stamp.HighPart=data.ftLastWriteTime.dwHighDateTime;
            files.push_back({directory+L"\\"+data.cFileName,stamp.QuadPart,data.nFileSizeLow});total+=data.nFileSizeLow;
        }while(FindNextFileW(search,&data));FindClose(search);
        std::sort(files.begin(),files.end(),[](const Old& a,const Old& b){return a.stamp<b.stamp;});
        FILETIME time;GetSystemTimeAsFileTime(&time);ULARGE_INTEGER now;now.LowPart=time.dwLowDateTime;now.HighPart=time.dwHighDateTime;
        for(auto& old:files)if(total>32*Limit||(now.QuadPart>old.stamp&&now.QuadPart-old.stamp>7ULL*24*60*60*10000000)){
            // Active writers deny delete sharing, so another host cannot lose its live log.
            if(DeleteFileW(old.path.c_str()))total-=old.bytes;
        }
    }
    void open(){file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file!=INVALID_HANDLE_VALUE){LARGE_INTEGER size;if(GetFileSizeEx(file,&size))fileBytes=(size_t)size.QuadPart;}}
public:
    ~Recorder(){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);}
    void configure(const std::wstring& root,int hostValue) noexcept {try{
        AcquireSRWLockExclusive(&lock);host=hostValue;
        if(path.empty()&&!root.empty()){
            directory=root+L"\\logs";CreateDirectoryW(directory.c_str(),nullptr);
            if(GetFileAttributesW(directory.c_str())&FILE_ATTRIBUTE_REPARSE_POINT){ReleaseSRWLockExclusive(&lock);return;}
            SYSTEMTIME time;GetSystemTime(&time);wchar_t name[120];swprintf_s(name,L"\\native-%04u%02u%02u-%02u%02u%02u-%03u-%lu.jsonl",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,GetCurrentProcessId());
            path=directory+name;prune();open();
        }ReleaseSRWLockExclusive(&lock);
    }catch(...){ReleaseSRWLockExclusive(&lock);}}
    void emit(const wchar_t* event,HRESULT hr,std::initializer_list<Number> fields) noexcept {
        if(!TryAcquireSRWLockExclusive(&lock)){InterlockedIncrement(&dropped);return;}
        try{
            SYSTEMTIME time;GetSystemTime(&time);char stamp[40];sprintf_s(stamp,"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds);
            std::string row="{\"schema\":1,\"version\":\"0.2.3-beta.1\",\"utc\":\""+std::string(stamp)+"\",\"tick\":"+std::to_string(GetTickCount64())+",\"runId\":"+std::to_string(runId)+",\"seq\":"+std::to_string(++sequence)+",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"tid\":"+std::to_string(GetCurrentThreadId())+" ,\"bits\":"+std::to_string(sizeof(void*)*8)+",\"host\":"+std::to_string(host)+",\"event\":\""+code(event)+"\",\"hr\":"+std::to_string((unsigned long)hr);
            row+=",\"traceId\":"+std::to_string(traceContext.id)+",\"parentTrace\":"+std::to_string(traceContext.parent)+",\"origin\":"+std::to_string(traceContext.origin)+",\"jobId\":"+std::to_string(traceContext.job);
            for(auto& field:fields){bool valid=field.key&&*field.key;for(const char* p=field.key;p&&*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')))valid=false;if(valid)row+=",\""+std::string(field.key)+"\":"+std::to_string(field.value);}
            row+="}\n";
            if(pending.size()+row.size()<=Buffer)pending+=row;else InterlockedIncrement(&dropped);
            recent.push_back(row);recentBytes+=row.size();while(recentBytes>128*1024){recentBytes-=recent.front().size();recent.pop_front();}
        }catch(...){InterlockedIncrement(&dropped);}ReleaseSRWLockExclusive(&lock);
    }
    void flush() noexcept {
        LONG loss=lost();if(InterlockedExchange(&reportedDropped,loss)!=loss)emit(L"log.droppedRecords",S_OK,{{"droppedRecords",loss}});
        if(!TryAcquireSRWLockExclusive(&lock))return;
        try{if(file!=INVALID_HANDLE_VALUE&&!pending.empty()){
            if(fileBytes+pending.size()>Limit){CloseHandle(file);file=INVALID_HANDLE_VALUE;
                DeleteFileW((path+L".3.jsonl").c_str());MoveFileExW((path+L".2.jsonl").c_str(),(path+L".3.jsonl").c_str(),MOVEFILE_REPLACE_EXISTING);MoveFileExW((path+L".1.jsonl").c_str(),(path+L".2.jsonl").c_str(),MOVEFILE_REPLACE_EXISTING);MoveFileExW(path.c_str(),(path+L".1.jsonl").c_str(),MOVEFILE_REPLACE_EXISTING);fileBytes=0;open();
            }
            DWORD written=0;if(file!=INVALID_HANDLE_VALUE&&fileBytes+pending.size()<=Limit&&WriteFile(file,pending.data(),(DWORD)pending.size(),&written,nullptr)){fileBytes+=written;pending.erase(0,written);}else InterlockedIncrement(&dropped);
        }}catch(...){InterlockedIncrement(&dropped);}ReleaseSRWLockExclusive(&lock);
    }
    std::wstring snapshot(){AcquireSRWLockExclusive(&lock);try{std::string text="[";for(auto& row:recent){if(text.size()>1)text+=",";text+=row;}text+="]";auto result=std::wstring(text.begin(),text.end());ReleaseSRWLockExclusive(&lock);return result;}catch(...){ReleaseSRWLockExclusive(&lock);throw;}}
    LONG lost()const{return InterlockedCompareExchange(const_cast<volatile LONG*>(&dropped),0,0);}
};
inline Recorder& recorder(){static Recorder value;return value;}
}
inline void log(const wchar_t* event,HRESULT hr=S_OK){runtimelog::recorder().emit(event,hr,{});}
inline void logEvent(const wchar_t* event,std::initializer_list<runtimelog::Number> fields,HRESULT hr=S_OK){runtimelog::recorder().emit(event,hr,fields);}
