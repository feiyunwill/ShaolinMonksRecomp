#include "legacy_folders.h"
#include <cctype>
#include <fstream>
#include <sstream>
#include <toml++/toml.hpp>
#include "file_ops.h"

namespace fs = std::filesystem;
namespace openshao::paths {
namespace {
bool Exists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}
// A stored value that names the previous folder, or anything inside it, follows the rename.
bool Rewrite(toml::node& node, const std::vector<LegacyFolder>& folders) {
    bool changed = false;
    if (auto* table = node.as_table()) {
        for (auto&& [key, value] : *table) changed |= Rewrite(value, folders);
    } else if (auto* array = node.as_array()) {
        for (auto& value : *array) changed |= Rewrite(value, folders);
    } else if (auto* text = node.as_string()) {
        const auto stored = files::FromUtf8(text->get()).lexically_normal();
        for (const auto& folder : folders) {
            if (folder.previous.empty() || folder.current.empty()) continue;
            const auto previous = folder.previous.lexically_normal();
            if (stored != previous && !files::Within(stored, previous)) continue;
            const auto relative = stored.lexically_relative(previous);
            const auto moved = relative.empty() || relative == "." ? folder.current : folder.current / relative;
            *text = files::Utf8(moved.lexically_normal());
            changed = true;
            break;
        }
    }
    return changed;
}
bool HasString(const fs::path& file, std::initializer_list<const char*> keys) {
    std::error_code ec;
    if (!fs::is_regular_file(file, ec)) return false;
    try {
        const auto table = toml::parse_file(files::Utf8(file));
        for (const char* key : keys) if (table[key].is_string()) return true;
    } catch (...) {}
    return false;
}
}

bool IsDataFolder(const fs::path& folder) {
    // The files written by every release since game selection and folder moves existed.
    if (HasString(folder / "game-source.toml", {"disc"}) || HasString(folder / "paths.toml", {"data", "saves"})) return true;
    std::error_code ec;
    const auto registry = folder / "mods" / "installed.toml";
    if (fs::is_regular_file(registry, ec)) {
        try {
            const auto table = toml::parse_file(files::Utf8(registry));
            if (const auto* mods = table["mods"].as_array(); mods && !mods->empty())
                if (const auto* first = mods->get(0)->as_table(); first && (*first)["manifest"].is_string()) return true;
        } catch (...) {}
    }
    // Launcher settings alone. Every release saves the master volume, the FPS display and a table of original game options.
    try {
        if (!fs::is_regular_file(folder / "settings.toml", ec)) return false;
        const auto table = toml::parse_file(files::Utf8(folder / "settings.toml"));
        return table.contains("master_volume") && table["show_fps"].is_boolean() && table["game"].is_table();
    } catch (...) { return false; }
}

bool IsSavesFolder(const fs::path& folder) {
    // Console saves keep their title folder under E/UDATA, and title data under E/TDATA.
    constexpr const char* kTitle = "4d570029";
    for (const char* area : {"UDATA", "TDATA"}) {
        std::error_code ec;
        const auto parent = folder / "hdd" / "E" / area;
        if (!fs::is_directory(parent, ec)) continue;
        for (fs::directory_iterator it(parent, ec), end; !ec && it != end; it.increment(ec)) {
            auto name = files::Utf8(it->path().filename());
            for (auto& ch : name) ch = char(std::tolower(static_cast<unsigned char>(ch)));
            if (name == kTitle && it->is_directory(ec)) return true;
        }
    }
    return false;
}

fs::path FindPreviousFolder(const fs::path& parent, const fs::path& current, const std::function<bool(const fs::path&)>& matches) {
    if (Exists(current)) return {};
    fs::path found;
    std::error_code ec;
    for (fs::directory_iterator it(parent, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code type;
        if (!it->is_directory(type) || it->is_symlink(type) || it->path() == current || !matches(it->path())) continue;
        // Two candidates leave the decision to the player; nothing moves.
        if (!found.empty()) return {};
        found = it->path();
    }
    return found;
}

std::vector<fs::path> AdoptLegacyFolders(const std::vector<LegacyFolder>& folders, bool move, std::string& warning) {
    std::vector<fs::path> result;
    for (const auto& folder : folders) {
        if (folder.previous.empty() || Exists(folder.current) || !Exists(folder.previous)) { result.push_back(folder.current); continue; }
        if (!move) { result.push_back(folder.previous); continue; }
        std::error_code ec;
        // Both names share a parent folder, so the rename never copies or deletes files.
        fs::rename(folder.previous, folder.current, ec);
        if (!ec) { result.push_back(folder.current); continue; }
        result.push_back(folder.previous);
        if (!warning.empty()) warning += "\n";
        warning += "Cannot rename " + files::Utf8(folder.previous) + " to " + files::Utf8(folder.current) +
                   " (" + ec.message() + "). The previous folder stays in use. Close other copies of the game and reopen it.";
    }
    return result;
}

bool RewriteLegacyPaths(const fs::path& file, const std::vector<LegacyFolder>& folders, std::string& error) {
    try {
        if (!fs::is_regular_file(file)) return true;
        auto table = toml::parse_file(files::Utf8(file));
        if (!Rewrite(table, folders)) return true;
        std::ostringstream text;
        text << table << "\n";
        files::AtomicWrite(file, text.str());
        return true;
    } catch (const std::exception& e) {
        error = "Cannot update " + files::Utf8(file) + ": " + e.what();
        return false;
    }
}
}
