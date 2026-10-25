#include "legacy_folders.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <toml++/toml.hpp>
#include "file_ops.h"
namespace fs = std::filesystem;
using openshao::paths::LegacyFolder;
void Require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
void Put(const fs::path& p, const std::string& text) { fs::create_directories(p.parent_path()); std::ofstream(p, std::ios::binary) << text; }
std::string Get(const fs::path& p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
std::string Quote(const fs::path& p) { return "'" + openshao::files::Utf8(p) + "'"; }
int main() {
    const auto base = fs::temp_directory_path() / ("openshao-legacy-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        using openshao::paths::FindPreviousFolder;
        using openshao::paths::IsDataFolder;
        using openshao::paths::IsSavesFolder;
        // Earlier folders are recognized by what they contain. Each signature is enough on its own.
        const auto samples = base / "Samples";
        Put(samples / "selection/game-source.toml", "disc = 'D:/game.iso'\n");
        Put(samples / "moved/paths.toml", "data = 'E:/Data'\nsaves = 'E:/Saves'\n");
        Put(samples / "mods/mods/installed.toml", "[[mods]]\nid = 'Pack'\nversion = '1.0.0'\nenabled = true\nmanifest = '{}'\n");
        Put(samples / "settings/settings.toml", "language = 'en'\nmaster_volume = 100\nshow_fps = false\n[game]\nvibration = true\n");
        for (const char* name : {"selection", "moved", "mods", "settings"})
            Require(IsDataFolder(samples / name), std::string("A data folder must be recognized by its ") + name);
        Put(samples / "other-app/settings.toml", "theme = 'dark'\nlanguage = 'en'\n");
        Put(samples / "other-selection/game-source.toml", "path = 'D:/game.iso'\n");
        Put(samples / "broken/game-source.toml", "disc = \n");
        fs::create_directories(samples / "empty");
        for (const char* name : {"other-app", "other-selection", "broken", "empty"})
            Require(!IsDataFolder(samples / name), std::string("An unrelated folder must not be taken: ") + name);
        Put(samples / "saves/hdd/E/UDATA/4D570029/save/data", "save");
        Put(samples / "title-data/hdd/E/TDATA/4d570029/cache", "cache");
        Put(samples / "other-game/hdd/E/UDATA/4d530004/save/data", "save");
        Put(samples / "no-title/hdd/E/UDATA/readme.txt", "text");
        Require(IsSavesFolder(samples / "saves") && IsSavesFolder(samples / "title-data"), "Saves must be recognized by the title folder");
        Require(!IsSavesFolder(samples / "other-game") && !IsSavesFolder(samples / "no-title") && !IsSavesFolder(samples / "settings"),
                "Other saves must not be taken");

        // One match beside unrelated folders is found; the current folder, two matches or none leave everything alone.
        const auto local = base / "Local";
        const LegacyFolder data{local / "Earlier name", local / "OpenShao"};
        const LegacyFolder saves{base / "Saved Games/Earlier saves", base / "Saved Games/OpenShao"};
        Put(local / "Unrelated/settings.toml", "theme = 'dark'\n");
        Put(local / "Another tool/cache.bin", "bytes");
        const auto custom = base / "Elsewhere/disc.iso";
        Put(data.previous / "settings.toml", "language = 'pt-BR'\n");
        Put(data.previous / "mods/installed.toml", "[[mods]]\nid = 'LanguagePack_PT-BR'\n");
        Put(data.previous / "games/game/default.xbe", "synthetic game");
        Put(data.previous / "paths.toml", "data = " + Quote(data.previous) + "\nsaves = " + Quote(saves.previous) +
                                          "\n[pending]\ndata = " + Quote(base / "Moved") + "\nsaves = " + Quote(saves.previous / "hdd") + "\n");
        Put(data.previous / "game-source.toml", "disc = " + Quote(data.previous / "games/game") + "\nother = " + Quote(custom) + "\n");
        Put(saves.previous / "hdd/E/UDATA/4d570029/campaign", "campaign bytes");
        Put(base / "Saved Games/Other game/readme.txt", "text");
        Require(FindPreviousFolder(local, data.current, IsDataFolder) == data.previous, "The earlier data folder must be found");
        Require(FindPreviousFolder(base / "Saved Games", saves.current, IsSavesFolder) == saves.previous, "The earlier saves folder must be found");
        Require(FindPreviousFolder(base / "Missing", data.current, IsDataFolder).empty(), "A missing parent has no earlier folder");
        Put(local / "Second copy/game-source.toml", "disc = 'D:/game.iso'\n");
        Require(FindPreviousFolder(local, data.current, IsDataFolder).empty(), "Two earlier folders must leave the choice to the player");
        fs::remove_all(local / "Second copy");

        std::string warning;
        auto adopted = openshao::paths::AdoptLegacyFolders({data, saves}, false, warning);
        Require(adopted == std::vector<fs::path>{data.previous, saves.previous}, "Read-only use must keep the previous folders");
        Require(fs::exists(data.previous) && !fs::exists(data.current), "Read-only use must not move folders");

        adopted = openshao::paths::AdoptLegacyFolders({data, saves}, true, warning);
        Require(warning.empty(), warning);
        Require(adopted == std::vector<fs::path>{data.current, saves.current}, "Moved folders must be used");
        Require(!fs::exists(data.previous) && !fs::exists(saves.previous), "Previous folder names must be free after the move");
        Require(Get(data.current / "settings.toml") == "language = 'pt-BR'\n", "Settings must survive the rename");
        Require(Get(data.current / "mods/installed.toml") == "[[mods]]\nid = 'LanguagePack_PT-BR'\n", "Installed mods must survive the rename");
        Require(Get(saves.current / "hdd/E/UDATA/4d570029/campaign") == "campaign bytes", "Saves must survive the rename");
        Require(FindPreviousFolder(local, data.current, IsDataFolder).empty(), "An existing current folder ends the search");

        std::string error;
        for (const auto& file : {data.current / "paths.toml", data.current / "game-source.toml"})
            Require(openshao::paths::RewriteLegacyPaths(file, {data, saves}, error), error);
        auto locations = toml::parse(Get(data.current / "paths.toml"));
        Require(fs::path(locations["data"].value_or(std::string{})) == data.current, "The stored data folder must follow the rename");
        Require(fs::path(locations["saves"].value_or(std::string{})) == saves.current, "The stored saves folder must follow the rename");
        Require(fs::path(locations["pending"]["data"].value_or(std::string{})) == base / "Moved", "Unrelated pending folders must stay unchanged");
        Require(fs::path(locations["pending"]["saves"].value_or(std::string{})) == saves.current / "hdd", "Paths inside a renamed folder must follow it");
        auto selection = toml::parse(Get(data.current / "game-source.toml"));
        Require(fs::path(selection["disc"].value_or(std::string{})) == data.current / "games/game", "Imported games must follow the rename");
        Require(fs::path(selection["other"].value_or(std::string{})) == custom, "Games outside the renamed folders must stay unchanged");

        const auto unchanged = Get(data.current / "settings.toml");
        Require(openshao::paths::RewriteLegacyPaths(data.current / "settings.toml", {data, saves}, error), error);
        Require(Get(data.current / "settings.toml") == unchanged, "Files without previous paths must keep their bytes");
        Require(openshao::paths::RewriteLegacyPaths(data.current / "missing.toml", {data, saves}, error), "A missing file needs no update");

        // A later start finds both names: the current folder wins and the previous one stays untouched.
        Put(data.previous / "settings.toml", "language = 'en'\n");
        adopted = openshao::paths::AdoptLegacyFolders({data}, true, warning);
        Require(adopted.front() == data.current && Get(data.current / "settings.toml") == unchanged, "Existing current data must win");
        Require(Get(data.previous / "settings.toml") == "language = 'en'\n", "A second previous folder must not be deleted or merged");

        // Normal restarts have no previous folder to migrate.
        const auto resume = data.current / "resume.toml";
        const std::string remembered = "# Keep this selection\ndisc = " + Quote(custom) +
            "\nlanguage = 'en'\n[paths]\ndata = " + Quote(data.current) + "\n";
        Put(resume, remembered);
        for (unsigned restart = 0; restart < 3; ++restart) {
            Require(openshao::paths::RewriteLegacyPaths(resume, {{{}, data.current}, {{}, saves.current}}, error), error);
            Require(Get(resume) == remembered, "Reopening without a previous folder must preserve the selected game and every stored value");
        }
        Put(resume, "disc = " + Quote(data.previous / "games/game") + "\nother = " + Quote(custom) + "\n");
        Require(openshao::paths::RewriteLegacyPaths(resume, {{{}, saves.current}, data}, error), error);
        const auto mixed = toml::parse(Get(resume));
        Require(fs::path(mixed["disc"].value_or(std::string{})) == data.current / "games/game" &&
                fs::path(mixed["other"].value_or(std::string{})) == custom,
                "An absent migration must not override a real rename or an external game path");

        // A fresh installation has no earlier folder.
        const LegacyFolder fresh{FindPreviousFolder(base / "Fresh", base / "Fresh/OpenShao", IsDataFolder), base / "Fresh/OpenShao"};
        adopted = openshao::paths::AdoptLegacyFolders({fresh}, true, warning);
        Require(fresh.previous.empty() && adopted.front() == fresh.current && !fs::exists(fresh.current),
                "A fresh installation must not create folders here");
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        std::error_code ignored; fs::remove_all(base, ignored);
        return 1;
    }
    std::error_code ignored; fs::remove_all(base, ignored);
    std::cout << "PASS: earlier folders are recognized by their contents and move to the OpenShao names with saves, mods and stored paths\n";
    return 0;
}
