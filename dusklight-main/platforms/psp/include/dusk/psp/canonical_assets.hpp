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

struct CanonicalStartupAssets {
    const char* sequence;
    const char* logos_ui;
    const char* title_ui;
    const char* file_select_ui;
    const char* title_room_model;
    const char* title_room_textures;
    const char* title_logo_model;
    const char* title_logo_textures;
    const char* title_logo_animation;
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
CanonicalAssetError resolve_canonical_startup_assets(
    CanonicalStartupAssets* output);
CanonicalAssetError resolve_canonical_room_assets(
    const save::StartContext& start,
    CanonicalRoomAssets* output);
const char* canonical_asset_error_name(CanonicalAssetError error);

}  // namespace dusk::psp::game

#endif
