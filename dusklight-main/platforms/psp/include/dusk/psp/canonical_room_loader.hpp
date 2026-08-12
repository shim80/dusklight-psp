#ifndef DUSK_PSP_CANONICAL_ROOM_LOADER_HPP
#define DUSK_PSP_CANONICAL_ROOM_LOADER_HPP

#include "dusk/psp/canonical_assets.hpp"
#include "dusk/psp/room_package.hpp"

#include <cstdint>

namespace dusk::psp::game {

struct OwnedPackage {
    std::uint8_t* bytes;
    std::uint32_t size;
    room::PackageView view;
};

struct CanonicalRoomPackages {
    OwnedPackage model;
    OwnedPackage textures;
    OwnedPackage collision;
    OwnedPackage scene;
};

enum class CanonicalRoomLoadError : std::uint8_t {
    Ok = 0,
    InvalidOutput,
    Path,
    FileSize,
    Allocation,
    Read,
    ModelPackage,
    TexturePackage,
    CollisionPackage,
    ScenePackage,
};

CanonicalRoomLoadError load_canonical_room_packages(
    const CanonicalRoomAssets& assets,
    CanonicalRoomPackages* packages);
void unload_canonical_room_packages(CanonicalRoomPackages* packages);
const char* canonical_room_load_error_name(CanonicalRoomLoadError error);

}  // namespace dusk::psp::game

#endif
