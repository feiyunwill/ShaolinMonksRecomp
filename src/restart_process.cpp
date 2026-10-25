#include "restart_process.h"
#include <algorithm>
#include <array>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace openshao::restart {
namespace {
std::string Utf8(const std::filesystem::path& path){const auto s=path.u8string();return {s.begin(),s.end()};}
#ifdef _WIN32
std::wstring Wide(const std::string& text){return std::filesystem::path(std::u8string(text.begin(),text.end())).wstring();}
std::wstring Quote(const std::wstring& text) {
    std::wstring result=L"\"";size_t slashes=0;
    for(const auto c:text) {
        if(c==L'\\'){++slashes;continue;}
        result.append(slashes*(c==L'"'?2:1),L'\\');slashes=0;
        if(c==L'"')result+=L'\\';result+=c;
    }
    result.append(slashes*2,L'\\');return result+L'"';
}
void Close(uintptr_t& handle){if(handle){CloseHandle(reinterpret_cast<HANDLE>(handle));handle=0;}}
#else
void Close(uintptr_t& handle){if(handle){close(int(handle)-1);handle=0;}}
#endif
}
bool Child::Prepare(const std::filesystem::path& executable,std::vector<std::string> args,
                    std::chrono::milliseconds timeout,std::string& error) {
    Cancel();error.clear();
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE reader=nullptr,writer=nullptr;
    if(!CreatePipe(&reader,&writer,&sa,0)){error="Cannot create the restart readiness pipe";return false;}
    SetHandleInformation(reader,HANDLE_FLAG_INHERIT,0);pipe_=reinterpret_cast<uintptr_t>(reader);
    args.insert(args.end(),{"--restart-parent",std::to_string(GetCurrentProcessId()),"--restart-ready-handle",std::to_string(reinterpret_cast<uintptr_t>(writer))});
    std::wstring line=Quote(executable.wstring());for(const auto& arg:args)line+=L" "+Quote(Wide(arg));
    std::vector<HANDLE> inherited{writer};
    auto standard=[&](DWORD kind,DWORD access) {
        HANDLE copy=nullptr,source=GetStdHandle(kind);
        if(!source || source==INVALID_HANDLE_VALUE || !DuplicateHandle(GetCurrentProcess(),source,GetCurrentProcess(),&copy,0,TRUE,DUPLICATE_SAME_ACCESS))
            copy=CreateFileW(L"NUL",access,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
        if(copy!=INVALID_HANDLE_VALUE && copy)inherited.push_back(copy);
        return copy;
    };
    STARTUPINFOEXW start{};start.StartupInfo.cb=sizeof(start);start.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    start.StartupInfo.hStdInput=standard(STD_INPUT_HANDLE,GENERIC_READ);
    start.StartupInfo.hStdOutput=standard(STD_OUTPUT_HANDLE,GENERIC_WRITE);
    start.StartupInfo.hStdError=standard(STD_ERROR_HANDLE,GENERIC_WRITE);
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    std::vector<uint8_t> attributes(bytes);start.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    bool initialized=InitializeProcThreadAttributeList(start.lpAttributeList,1,0,&bytes)!=FALSE;
    bool configured=initialized && UpdateProcThreadAttribute(start.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        inherited.data(),inherited.size()*sizeof(HANDLE),nullptr,nullptr)!=FALSE;
    PROCESS_INFORMATION process{};
    const bool launched=configured && CreateProcessW(executable.c_str(),line.data(),nullptr,nullptr,TRUE,
        EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW,nullptr,executable.parent_path().c_str(),&start.StartupInfo,&process)!=FALSE;
    const auto launch_error=GetLastError();
    if(initialized)DeleteProcThreadAttributeList(start.lpAttributeList);
    for(auto handle:inherited)CloseHandle(handle);
    if(!launched){error="Cannot start the replacement process (error "+std::to_string(launch_error)+")";Cancel();return false;}
    CloseHandle(process.hThread);process_=reinterpret_cast<uintptr_t>(process.hProcess);pid_=process.dwProcessId;
#else
    int channel[2];if(pipe2(channel,O_CLOEXEC|O_NONBLOCK)!=0){error="Cannot create the restart readiness pipe";return false;}
    args.insert(args.end(),{"--restart-parent",std::to_string(getpid()),"--restart-ready-handle",std::to_string(channel[1])});
    const auto file=Utf8(executable);std::vector<char*> pointers{const_cast<char*>(file.c_str())};
    for(auto& arg:args)pointers.push_back(arg.data());pointers.push_back(nullptr);
    const auto child=fork();
    if(child==0) {
        close(channel[0]);fcntl(channel[1],F_SETFD,0);
        execv(file.c_str(),pointers.data());
        constexpr char failure[]="EThe replacement executable could not be started\n";
        const auto ignored=write(channel[1],failure,sizeof(failure)-1);(void)ignored;_exit(127);
    }
    close(channel[1]);pipe_=uintptr_t(channel[0])+1;
    if(child<0){error="Cannot start the replacement process";Cancel();return false;}
    pid_=uint32_t(child);
#endif
    const auto deadline=std::chrono::steady_clock::now()+timeout;std::string response;
    while(std::chrono::steady_clock::now()<deadline) {
        char buffer[512];size_t count=0;bool exited=false;
#ifdef _WIN32
        DWORD available=0,read=0;
        if(PeekNamedPipe(reinterpret_cast<HANDLE>(pipe_),nullptr,0,nullptr,&available,nullptr) && available &&
           ReadFile(reinterpret_cast<HANDLE>(pipe_),buffer,std::min<DWORD>(available,sizeof(buffer)),&read,nullptr))count=read;
        exited=WaitForSingleObject(reinterpret_cast<HANDLE>(process_),0)==WAIT_OBJECT_0;
#else
        const auto read_count=read(int(pipe_)-1,buffer,sizeof(buffer));if(read_count>0)count=size_t(read_count);
        int status=0;if(waitpid(pid_t(pid_),&status,WNOHANG)==pid_t(pid_)){pid_=0;exited=true;}
#endif
        response.append(buffer,count);
        if(const auto newline=response.find('\n');newline!=std::string::npos) {
            if(response[0]=='O'){Close(pipe_);return true;}
            error=response.substr(1,newline-1);Cancel();return false;
        }
        if(exited){error="The replacement process stopped before confirming its resources";Cancel();return false;}
        if(response.size()>4096){error="Invalid restart readiness response";Cancel();return false;}
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    error="The replacement process did not finish preparing its resources";Cancel();return false;
}
bool Child::Running() {
#ifdef _WIN32
    return process_ && WaitForSingleObject(reinterpret_cast<HANDLE>(process_),0)==WAIT_TIMEOUT;
#else
    if(!pid_)return false;
    int status=0;const auto result=waitpid(pid_t(pid_),&status,WNOHANG);
    if(result==0)return true;
    if(result==pid_t(pid_) || (result<0 && errno==ECHILD))pid_=0;
    return false;
#endif
}
void Child::Cancel() {
#ifdef _WIN32
    if(process_ && WaitForSingleObject(reinterpret_cast<HANDLE>(process_),0)==WAIT_TIMEOUT) {
        TerminateProcess(reinterpret_cast<HANDLE>(process_),9);WaitForSingleObject(reinterpret_cast<HANDLE>(process_),5000);
    }
#else
    if(pid_) {
        kill(pid_t(pid_),SIGTERM);int status=0;
        for(unsigned i=0;i<100;++i) {
            if(waitpid(pid_t(pid_),&status,WNOHANG)==pid_t(pid_)){pid_=0;break;}
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if(pid_){kill(pid_t(pid_),SIGKILL);while(waitpid(pid_t(pid_),&status,0)<0 && errno==EINTR){}}
    }
#endif
    Release();
}
void Child::Release(){Close(pipe_);
#ifdef _WIN32
    Close(process_);
#endif
    pid_=0;
}
bool AwaitParent(uint32_t parent,uintptr_t ready,const std::function<bool(std::string&)>& preflight,
                 std::chrono::milliseconds timeout,std::string& error) {
#ifdef _WIN32
    HANDLE old=OpenProcess(SYNCHRONIZE,FALSE,parent),channel=reinterpret_cast<HANDLE>(ready);
    bool valid=old && GetFileType(channel)==FILE_TYPE_PIPE;
#else
    int channel=int(ready);bool valid=parent>1 && getppid()==pid_t(parent) && fcntl(channel,F_GETFD)>=0;
#endif
    bool checked=false;
    if(valid)try{checked=preflight(error);}catch(const std::exception& e){error=e.what();}
    if(!valid)error="Invalid restart parent or readiness pipe";
    std::string response=checked?"O\n":"E"+(error.empty()?std::string("The language resources could not be prepared"):error)+"\n";
    if(response.size()>4096)response="EThe language resources could not be prepared\n";
#ifdef _WIN32
    DWORD written=0;const bool sent=valid && WriteFile(channel,response.data(),DWORD(response.size()),&written,nullptr) && written==response.size();
    if(channel && channel!=INVALID_HANDLE_VALUE)CloseHandle(channel);
    const bool ended=checked && sent && WaitForSingleObject(old,DWORD(timeout.count()))==WAIT_OBJECT_0;
    if(old)CloseHandle(old);
#else
    const bool sent=valid && write(channel,response.data(),response.size())==ssize_t(response.size());close(channel);
    const auto deadline=std::chrono::steady_clock::now()+timeout;
    while(checked && sent && getppid()==pid_t(parent) && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const bool ended=checked && sent && getppid()!=pid_t(parent);
#endif
    if(!ended && error.empty())error="The previous session did not release its resources";
    return ended;
}
}
