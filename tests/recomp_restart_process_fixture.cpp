#include "restart_process.h"
#include <fstream>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif
int main(int argc,char** argv) {
    std::vector<std::string> args;
#ifdef _WIN32
    int count=0;auto wide=CommandLineToArgvW(GetCommandLineW(),&count);
    for(int i=0;i<count;++i) {
        int length=WideCharToMultiByte(CP_UTF8,0,wide[i],-1,nullptr,0,nullptr,nullptr);std::string value(size_t(length),0);
        WideCharToMultiByte(CP_UTF8,0,wide[i],-1,value.data(),length,nullptr,nullptr);value.pop_back();args.push_back(std::move(value));
    }
    LocalFree(wide);
#else
    for(int i=0;i<argc;++i)args.emplace_back(argv[i]);
#endif
    auto value=[&](const std::string& key){for(size_t i=1;i+1<args.size();++i)if(args[i]==key)return args[i+1];return std::string{};};
    const auto root=value("--root");
    const auto directory=std::filesystem::path(std::u8string(root.begin(),root.end()));
    const auto mode=value("--mode");
    if(args.size()>1 && args[1]=="--helper-child") {
        if(mode=="exit")return 21;
        std::string error;
        const bool done=openshao::restart::AwaitParent(uint32_t(std::stoul(value("--restart-parent"))),uintptr_t(std::stoull(value("--restart-ready-handle"))),[&](std::string& message) {
            if(mode=="hang")std::this_thread::sleep_for(std::chrono::seconds(10));
            if(mode=="reject"){message="Fixture resources rejected";return false;}
            std::ofstream(directory/"argument.txt",std::ios::binary)<<value("--text");
            std::ofstream(directory/"prepared.txt")<<"ready";return true;
        },mode=="expired"?std::chrono::milliseconds(30):std::chrono::seconds(5),error);
        if(done)std::ofstream(directory/"resumed.txt")<<"parent exited";
        return done?0:22;
    }
    std::vector<std::string> child_args{"--helper-child","--root",value("--root"),"--mode",mode,"--text",value("--text")};
    const auto executable=std::filesystem::path(std::u8string(args[0].begin(),args[0].end()));
    openshao::restart::Child child;std::string error;
    const bool ready=child.Prepare(mode=="missing"?directory/"absent.exe":executable,child_args,std::chrono::milliseconds(500),error);
    if(mode=="reject" || mode=="exit" || mode=="missing" || mode=="hang")return ready?1:0;
    if(!ready)return 2;
    if(!child.Running())return 3;
    if(mode=="expired") {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(child.Running())return 4;
        child.Cancel();return 0;
    }
    if(mode=="cancel"){child.Cancel();if(child.Running())return 5;}else child.Release();
    std::ofstream(directory/"coordinator.txt")<<"closed";return 0;
}
