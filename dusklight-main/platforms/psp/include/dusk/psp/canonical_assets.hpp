#ifndef DUSK_PSP_CANONICAL_ASSETS_HPP
#define DUSK_PSP_CANONICAL_ASSETS_HPP

#include "dusk/psp/save_runtime.hpp"

namespace dusk::psp::game {

struct CanonicalCommonAssets {
    const char* link_model;
    const char* link_textures;
    const char* link_animations;
    const char* hud_ui;
};

struct CanonicalRoomAssets {
    const char* model;
    const char* textures;
    const char* collision;
    const char* scene;
};

enum class CanonicalAssetError {
    Ok = 0,
    UnsupportedStage,
    UnsupportedRoom,
    UnsupportedLayer,
    UnsupportedStartPoint,
    NullOutput,
};

CanonicalAssetError resolve_canonical_common_assets(
    CanonicalCommonAssets* output);
CanonicalAssetError resolve_canonical_room_assets(
    const save::StartContext& start,
    CanonicalRoomAssets* output);
const char* canonical_asset_error_name(CanonicalAssetError error);

}  // namespace dusk::psp::game

#endif
