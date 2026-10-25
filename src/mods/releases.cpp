#include "browser.h"
#include "version.h"
namespace openshao::mods {
const Release* LatestCompatible(const Mod& mod) {
    if(!mod.unsupported_reason.empty())return nullptr;
    for(const auto& release:mod.versions)if(CompareVersions(release.recomp,RecompVersion)<=0)return &release;
    return nullptr;
}
}
