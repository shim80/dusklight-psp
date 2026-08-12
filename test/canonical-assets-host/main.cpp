#include "dusk/psp/canonical_assets.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

namespace game = dusk::psp::game;
namespace save = dusk::psp::save;

int main() {
    const save::StartContext start = save::default_new_game_start();
    assert(std::strcmp(start.stage.data(), "F_SP108") == 0);
    assert(start.room == 1);
    assert(start.start_point == 21);
    assert(start.layer == 0);

    game::CanonicalRoomAssets assets = {};
    assert(game::resolve_canonical_room_assets(start, &assets) ==
           game::CanonicalAssetError::Ok);
    assert(std::strcmp(assets.model, "data/stages/F_SP108/R01/room.dprm") == 0);
    assert(std::strcmp(assets.textures, "data/stages/F_SP108/R01/room.dptx") == 0);
    assert(std::strcmp(assets.collision, "data/stages/F_SP108/R01/room.dpcl") == 0);
    assert(std::strcmp(assets.scene, "data/stages/F_SP108/R01/room.dpsc") == 0);

    save::StartContext unsupported = start;
    unsupported.room = 2;
    assert(game::resolve_canonical_room_assets(unsupported, &assets) ==
           game::CanonicalAssetError::UnsupportedRoom);
    unsupported = start;
    unsupported.start_point = 20;
    assert(game::resolve_canonical_room_assets(unsupported, &assets) ==
           game::CanonicalAssetError::UnsupportedStartPoint);
    unsupported = start;
    unsupported.layer = 1;
    assert(game::resolve_canonical_room_assets(unsupported, &assets) ==
           game::CanonicalAssetError::UnsupportedLayer);
    unsupported = start;
    std::memcpy(unsupported.stage.data(), "D_MN05A", 8);
    assert(game::resolve_canonical_room_assets(unsupported, &assets) ==
           game::CanonicalAssetError::UnsupportedStage);
    assert(game::resolve_canonical_room_assets(start, nullptr) ==
           game::CanonicalAssetError::NullOutput);

    std::puts(
        "CANONICAL_ASSETS_HOST_OK stage=F_SP108 room=1 start=21 layer=0 "
        "paths=4 fail_closed=1");
    return 0;
}
