#include "graphics_cache.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace openshao::graphics {
namespace {
constexpr uint64_t kMagic = 0x3145484341434D53ull;
constexpr uint64_t kFileLimit = 256ull * 1024 * 1024;
constexpr uint64_t kRecordLimit = 32ull * 1024 * 1024;
bool Seek(HANDLE file, uint64_t offset) {
    LARGE_INTEGER position{};
    position.QuadPart = offset;
    return SetFilePointerEx(file, position, nullptr, FILE_BEGIN) != FALSE;
}
bool Read(HANDLE file, void* bytes, DWORD size) {
    DWORD count = 0;
    return ReadFile(file, bytes, size, &count, nullptr) && count == size;
}
bool Write(HANDLE file, const void* bytes, DWORD size) {
    DWORD count = 0;
    return WriteFile(file, bytes, size, &count, nullptr) && count == size;
}
}

uint64_t CacheHash(std::span<const uint8_t> bytes, uint64_t seed) {
    for (uint8_t byte : bytes) seed = (seed ^ byte) * 1099511628211ull;
    return seed;
}

struct CacheFile::Impl {
    HANDLE file = INVALID_HANDLE_VALUE;
    bool writable = false;
    bool stop = false;
    bool busy = false;
    size_t loaded = 0;
    uint64_t reserved = 0;
    std::atomic<uint64_t> saved = 0, errors = 0;
    mutable std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable drained;
    std::unordered_map<uint64_t, std::vector<uint8_t>> records;
    std::deque<std::pair<uint64_t, std::vector<uint8_t>>> pending;
    std::thread writer;

    Impl(const std::filesystem::path& path, uint64_t version) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        writable = file != INVALID_HANDLE_VALUE;
        if (!writable) {
            file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        }
        if (file == INVALID_HANDLE_VALUE) { errors++; return; }
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file, &size)) { writable = false; errors++; return; }
        const std::array<uint64_t, 2> expected{kMagic, version};
        std::array<uint64_t, 2> header{};
        uint64_t good = sizeof(header);
        if (size.QuadPart < sizeof(header) || uint64_t(size.QuadPart) > kFileLimit ||
            !Read(file, header.data(), sizeof(header)) || header != expected) {
            if (!writable) return;
            if (!Seek(file, 0) || !SetEndOfFile(file) || !Write(file, expected.data(), sizeof(expected))) {
                writable = false; errors++; return;
            }
            size.QuadPart = good;
        }
        while (good < uint64_t(size.QuadPart)) {
            std::array<uint64_t, 3> record{};  // key, byte count, checksum
            if (uint64_t(size.QuadPart) - good < sizeof(record) || !Read(file, record.data(), sizeof(record))) break;
            const auto [key, count, checksum] = record;
            if (!count || count > kRecordLimit || count > uint64_t(size.QuadPart) - good - sizeof(record) ||
                records.size() >= 200000) break;
            std::vector<uint8_t> bytes(size_t(count), uint8_t{});
            if (!Read(file, bytes.data(), DWORD(count)) || CacheHash(bytes, key) != checksum) break;
            records[key] = std::move(bytes);
            good += sizeof(record) + count;
        }
        loaded = records.size();
        reserved = good;
        // Only the exclusive writer can repair an interrupted or corrupt tail.
        if (writable && (!Seek(file, good) || !SetEndOfFile(file))) { writable = false; errors++; }
        if (writable) writer = std::thread([this] { Run(); });
    }

    void Run() {
        for (;;) {
            std::pair<uint64_t, std::vector<uint8_t>> item;
            {
                std::unique_lock lock(mutex);
                ready.wait(lock, [this] { return stop || !pending.empty(); });
                if (pending.empty()) break;
                item = std::move(pending.front());
                pending.pop_front();
                busy = true;
            }
            const auto& [key, bytes] = item;
            const std::array<uint64_t, 3> record{key, bytes.size(), CacheHash(bytes, key)};
            if (!Write(file, record.data(), sizeof(record)) || !Write(file, bytes.data(), DWORD(bytes.size()))) {
                std::lock_guard lock(mutex);
                errors++;
                writable = false;
                pending.clear();
                busy = false;
                drained.notify_all();
                break;
            }
            saved++;
            {
                std::lock_guard lock(mutex);
                busy = false;
                drained.notify_all();
            }
        }
    }

    ~Impl() {
        {
            std::lock_guard lock(mutex);
            stop = true;
        }
        ready.notify_one();
        if (writer.joinable()) writer.join();
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    }
};

CacheFile::CacheFile(const std::filesystem::path& file, uint64_t version) : impl_(std::make_unique<Impl>(file, version)) {}
CacheFile::~CacheFile() = default;
std::vector<uint8_t> CacheFile::Get(uint64_t key) const {
    std::lock_guard lock(impl_->mutex);
    const auto it = impl_->records.find(key);
    return it == impl_->records.end() ? std::vector<uint8_t>{} : it->second;
}
bool CacheFile::Put(uint64_t key, std::span<const uint8_t> bytes) {
    std::lock_guard lock(impl_->mutex);
    if (!impl_->writable || bytes.empty() || bytes.size() > kRecordLimit ||
        impl_->reserved + 24 + bytes.size() > kFileLimit) return false;
    impl_->reserved += 24 + bytes.size();
    std::vector<uint8_t> copy(bytes.begin(), bytes.end());
    impl_->records[key] = copy;
    impl_->pending.emplace_back(key, std::move(copy));
    impl_->ready.notify_one();
    return true;
}
size_t CacheFile::Loaded() const { return impl_->loaded; }
void CacheFile::Flush() {
    std::unique_lock lock(impl_->mutex);
    impl_->drained.wait(lock, [this] { return impl_->pending.empty() && !impl_->busy; });
}
uint64_t CacheFile::Saved() const { return impl_->saved; }
uint64_t CacheFile::Errors() const { return impl_->errors; }
bool CacheFile::Writable() const { std::lock_guard lock(impl_->mutex); return impl_->writable; }
}
