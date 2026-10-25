#include "graphics_cache.h"
#include <windows.h>
#include <cstdio>
#include <fstream>
#include <stdexcept>

using openshao::graphics::CacheFile;
using Bytes = std::vector<uint8_t>;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("openshao-graphics-cache-test-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    try {
        const auto file = root / "cache.bin";
        {
            CacheFile cache(file, 123);
            Require(cache.Get(7).empty(), "a new cache must miss");
            Require(cache.Put(7, Bytes{1, 2, 3, 4}), "enqueue a compiled shader");
            cache.Flush();
            Require(cache.Saved() == 1 && cache.Errors() == 0, "report completed background writes");
        }
        {
            CacheFile cache(file, 123);
            Require(cache.Get(7) == Bytes({1, 2, 3, 4}), "compiled shaders must survive reopening the cache");
            Require(cache.Loaded() == 1, "report records restored from disk");
            CacheFile second(file, 123);
            Require(second.Get(7) == Bytes({1, 2, 3, 4}), "another instance can read a cache in use");
            Require(!second.Put(8, Bytes{9}), "another instance must not corrupt the active writer");
        }
        {
            std::ofstream append(file, std::ios::binary | std::ios::app);
            append << "incomplete";
        }
        {
            CacheFile cache(file, 123);
            Require(cache.Get(7) == Bytes({1, 2, 3, 4}), "an interrupted tail must preserve completed records");
            Require(cache.Put(8, Bytes{5, 6}), "repair the interrupted tail before appending");
        }
        {
            CacheFile cache(file, 123);
            Require(cache.Get(8) == Bytes({5, 6}), "records after a repaired tail must survive reopening");
        }
        {
            CacheFile cache(file, 124);
            Require(cache.Get(7).empty(), "an incompatible renderer or driver must miss");
            Require(cache.Put(9, Bytes{8}), "an incompatible cache can be rebuilt");
        }
        {
            std::fstream corrupt(file, std::ios::binary | std::ios::in | std::ios::out);
            corrupt.seekp(-1, std::ios::end);
            corrupt.put('\0');
        }
        {
            CacheFile cache(file, 124);
            Require(cache.Get(9).empty(), "corrupt bytecode must never be returned to the driver");
            Require(!cache.Put(10, {}), "empty compilation results must not be cached");
        }
        {
            std::ofstream blocker(root / "blocked");
            blocker << "fixture";
        }
        {
            CacheFile cache(root / "blocked" / "cache.bin", 1);
            Require(cache.Get(1).empty() && !cache.Put(1, Bytes{1}), "cache I/O failure must allow a normal cache miss");
        }
        std::filesystem::remove_all(root);
        std::puts("PASS: persistent graphics records, corruption recovery, versioning, concurrent readers and I/O failure");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        std::filesystem::remove_all(root);
        return 1;
    }
}
