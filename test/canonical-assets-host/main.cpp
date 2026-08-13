#include "dusk/psp/canonical_assets.hpp"

#include <cstdio>
#include <cstring>

namespace game = dusk::psp::game;
namespace save = dusk::psp::save;

namespace {

bool expect(bool condition, const char* message) {
    if (condition) {
        return true;
    }
    std::fprintf(stderr, "CANONICAL_ASSETS_HOST_FAIL %s\n", message);
    return false;
}

}  // namespace

int main() {
    game::CanonicalCommonAssets common = {};
    if (!expect(
            game::resolve_canonical_common_assets(&common) ==
                game::CanonicalAssetError::Ok,
            "resolve_common") ||
        !expect(std::strcmp(common.link_model, "data/common/link.dpsk") == 0,
                "link_model_path") ||
        !expect(std::strcmp(common.link_textures, "data/common/link.dptx") == 0,
                "link_texture_path") ||
        !expect(std::strcmp(common.link_animations, "data/common/link.dpan") == 0,
                "link_animation_path") ||
        !expect(std::strcmp(common.hud_ui, "data/common/hud.dpui") == 0,
                "hud_path") ||
        !expect(
            game::resolve_canonical_common_assets(nullptr) ==
                game::CanonicalAssetError::NullOutput,
            "common_null_output")) {
        return 1;
    }

    game::CanonicalStartupAssets startup = {};
    if (!expect(
            game::resolve_canonical_startup_assets(&startup) ==
                game::CanonicalAssetError::Ok,
            "resolve_startup") ||
        !expect(std::strcmp(startup.sequence, "data/startup/startup.dpst") == 0,
                "startup_sequence_path") ||
        !expect(std::strcmp(startup.logos_ui, "data/startup/startup_logos.dpsu") == 0,
                "startup_logos_path") ||
        !expect(std::strcmp(startup.title_ui, "data/startup/title_ui.dpsu") == 0,
                "title_ui_path") ||
        !expect(std::strcmp(startup.file_select_ui, "data/startup/file_select.dpsu") == 0,
                "file_select_ui_path") ||
        !expect(std::strcmp(startup.title_room_model, "data/startup/title_room.dprm") == 0,
                "title_room_model_path") ||
        !expect(std::strcmp(startup.title_room_textures, "data/startup/title_room.dptx") == 0,
                "title_room_texture_path") ||
        !expect(std::strcmp(startup.title_logo_model, "data/startup/title_logo.dprm") == 0,
                "title_logo_model_path") ||
        !expect(std::strcmp(startup.title_logo_textures, "data/startup/title_logo.dptx") == 0,
                "title_logo_texture_path") ||
        !expect(std::strcmp(startup.title_logo_animation, "data/startup/title_logo.dpan") == 0,
                "title_logo_animation_path") ||
        !expect(
            game::resolve_canonical_startup_assets(nullptr) ==
                game::CanonicalAssetError::NullOutput,
            "startup_null_output")) {
        return 2;
    }

    const save::StartContext start = save::default_new_game_start();
    if (!expect(std::strcmp(start.stage.data(), "F_SP108") == 0, "stage") ||
        !expect(start.room == 1, "room") ||
        !expect(start.start_point == 21, "start_point") ||
        !expect(start.layer == 0, "layer")) {
        return 3;
    }

    game::CanonicalRoomAssets assets = {};
    if (!expect(
            game::resolve_canonical_room_assets(start, &assets) ==
                game::CanonicalAssetError::Ok,
            "resolve_default") ||
        !expect(
            std::strcmp(assets.model, "data/stages/F_SP108/R01/room.dprm") == 0,
            "model_path") ||
        !expect(
            std::strcmp(assets.textures, "data/stages/F_SP108/R01/room.dptx") == 0,
            "texture_path") ||
        !expect(
            std::strcmp(assets.collision, "data/stages/F_SP108/R01/room.dpcl") == 0,
            "collision_path") ||
        !expect(
            std::strcmp(assets.scene, "data/stages/F_SP108/R01/room.dpsc") == 0,
            "scene_path")) {
        return 4;
    }

    save::StartContext unsupported = start;
    unsupported.room = 2;
    if (!expect(
            game::resolve_canonical_room_assets(unsupported, &assets) ==
                game::CanonicalAssetError::UnsupportedRoom,
            "unsupported_room")) {
        return 5;
    }
    unsupported = start;
    unsupported.start_point = 20;
    if (!expect(
            game::resolve_canonical_room_assets(unsupported, &assets) ==
                game::CanonicalAssetError::UnsupportedStartPoint,
            "unsupported_start")) {
        return 6;
    }
    unsupported = start;
    unsupported.layer = 1;
    if (!expect(
            game::resolve_canonical_room_assets(unsupported, &assets) ==
                game::CanonicalAssetError::UnsupportedLayer,
            "unsupported_layer")) {
        return 7;
    }
    unsupported = start;
    std::memcpy(unsupported.stage.data(), "D_MN05A", 8);
    if (!expect(
            game::resolve_canonical_room_assets(unsupported, &assets) ==
                game::CanonicalAssetError::UnsupportedStage,
            "unsupported_stage") ||
        !expect(
            game::resolve_canonical_room_assets(start, nullptr) ==
                game::CanonicalAssetError::NullOutput,
            "null_output")) {
        return 8;
    }

    std::puts(
        "CANONICAL_STARTUP_ASSETS_HOST_OK paths=9 "
        "sequence=DPST ui=DPSU title=DPRM,DPTX,DPAN fail_closed=1");
    std::puts(
        "CANONICAL_ASSETS_HOST_OK stage=F_SP108 room=1 start=21 layer=0 "
        "paths=4 fail_closed=1");
    return 0;
}
