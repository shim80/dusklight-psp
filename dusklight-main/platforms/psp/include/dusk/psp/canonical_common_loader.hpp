#ifndef DUSK_PSP_CANONICAL_COMMON_LOADER_HPP
#define DUSK_PSP_CANONICAL_COMMON_LOADER_HPP

#include "dusk/psp/canonical_assets.hpp"
#include "dusk/psp/playable_package.hpp"

#include <cstdint>

namespace dusk::psp::game {

struct OwnedPlayablePackage {
    std::uint8_t* bytes;
    std::uint32_t size;
    playable::PackageView view;
};

struct CanonicalCommonPackages {
    OwnedPlayablePackage link_model;
    OwnedPlayablePackage link_textures;
    OwnedPlayablePackage link_animations;
    OwnedPlayablePackage hud_ui;
};

enum class CanonicalCommonLoadError : std::uint8_t {
    Ok = 0,
    InvalidOutput,
    Path,
    FileSize,
    Allocation,
    Read,
    LinkModelPackage,
    LinkTexturePackage,
    LinkAnimationPackage,
    HudPackage,
};

CanonicalCommonLoadError load_canonical_common_packages(
    const CanonicalCommonAssets& assets,
    CanonicalCommonPackages* packages);
void unload_canonical_common_packages(CanonicalCommonPackages* packages);
playable::PackageSet canonical_playable_package_set(
    const CanonicalCommonPackages& packages);
const char* canonical_common_load_error_name(CanonicalCommonLoadError error);

}  // namespace dusk::psp::game

#endif
