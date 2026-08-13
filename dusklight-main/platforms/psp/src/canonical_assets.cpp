#include "dusk/psp/canonical_assets.hpp"

#include <cstring>

namespace dusk::psp::game {
namespace {

constexpr CanonicalCommonAssets kCommonAssets = {
    "data/common/link.dpsk",
    "data/common/link.dptx",
    "data/common/link.dpan",
    "data/common/hud.dpui",
};
constexpr char kFsp108[] = "F_SP108";
constexpr CanonicalRoomAssets kFsp108R01Assets = {
    "data/stages/F_SP108/R01/room.dprm",
    "data/stages/F_SP108/R01/room.dptx",
    "data/stages/F_SP108/R01/room.dpcl",
    "data/stages/F_SP108/R01/room.dpsc",
};

bool stage_equals(const save::StartContext& start, const char* expected) {
    const std::size_t expected_size = std::strlen(expected);
    if (expected_size >= start.stage.size()) {
        return false;
    }
    return std::memcmp(start.stage.data(), expected, expected_size) == 0 &&
           start.stage[expected_size] == '\0';
}

}  // namespace

CanonicalAssetError resolve_canonical_common_assets(
    CanonicalCommonAssets* output) {
    if (output == nullptr) {
        return CanonicalAssetError::NullOutput;
    }
    *output = kCommonAssets;
    return CanonicalAssetError::Ok;
}

CanonicalAssetError resolve_canonical_room_assets(
    const save::StartContext& start,
    CanonicalRoomAssets* output) {
    if (output == nullptr) {
        return CanonicalAssetError::NullOutput;
    }
    *output = {};

    if (!stage_equals(start, kFsp108)) {
        return CanonicalAssetError::UnsupportedStage;
    }
    if (start.room != 1) {
        return CanonicalAssetError::UnsupportedRoom;
    }
    if (start.layer != 0) {
        return CanonicalAssetError::UnsupportedLayer;
    }
    if (start.start_point != 21) {
        return CanonicalAssetError::UnsupportedStartPoint;
    }

    *output = kFsp108R01Assets;
    return CanonicalAssetError::Ok;
}

const char* canonical_asset_error_name(CanonicalAssetError error) {
    switch (error) {
    case CanonicalAssetError::Ok: return "ok";
    case CanonicalAssetError::UnsupportedStage: return "unsupported_stage";
    case CanonicalAssetError::UnsupportedRoom: return "unsupported_room";
    case CanonicalAssetError::UnsupportedLayer: return "unsupported_layer";
    case CanonicalAssetError::UnsupportedStartPoint: return "unsupported_start_point";
    case CanonicalAssetError::NullOutput: return "null_output";
    }
    return "unknown";
}

}  // namespace dusk::psp::game
