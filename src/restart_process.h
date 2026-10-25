#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace openshao::restart {
class Child {
public:
    Child()=default;
    ~Child(){Cancel();}
    Child(const Child&)=delete;
    Child& operator=(const Child&)=delete;
    bool Prepare(const std::filesystem::path& executable,std::vector<std::string> arguments,
                 std::chrono::milliseconds timeout,std::string& error);
    bool Running();
    void Cancel();
    void Release();
private:
    uintptr_t process_=0,pipe_=0;
    uint32_t pid_=0;
};
// The child checks its resources before acknowledging readiness. It must not acquire the parent's data/save locks.
bool AwaitParent(uint32_t parent,uintptr_t ready,const std::function<bool(std::string&)>& preflight,
                 std::chrono::milliseconds timeout,std::string& error);
}
