#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace openshao::graphics {
class CacheFile {
public:
    CacheFile(const std::filesystem::path& file, uint64_t version);
    ~CacheFile();
    CacheFile(const CacheFile&) = delete;
    CacheFile& operator=(const CacheFile&) = delete;
    std::vector<uint8_t> Get(uint64_t key) const;
    bool Put(uint64_t key, std::span<const uint8_t> bytes);
    void Flush();
    size_t Loaded() const;
    uint64_t Saved() const;
    uint64_t Errors() const;
    bool Writable() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
uint64_t CacheHash(std::span<const uint8_t> bytes, uint64_t seed = 14695981039346656037ull);
}
