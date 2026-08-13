#include "dusk/psp/canonical_common_loader.hpp"

#include "dusk/psp/platform.hpp"

#include <cstdlib>
#include <cstring>

namespace dusk::psp::game {
namespace {

using Validator = playable::PackageError (*)(
    const void*, std::uint32_t, playable::PackageView*);

bool package_range(
    std::uint32_t offset,
    std::uint32_t count,
    std::uint32_t stride,
    std::uint32_t size) {
    return offset <= size && stride != 0 &&
        count <= (size - offset) / stride;
}

playable::PackageError validate_canonical_hud_dpui(
    const void* source,
    std::uint32_t size,
    playable::PackageView* view) {
    const playable::PackageError strict =
        playable::validate_dpui(source, size, view);
    if (strict == playable::PackageError::Ok) {
        return strict;
    }

    // The preserved canonical asset set predates the later DPUI-v2 contract
    // that requires the complete printable Rodan glyph range. Its compact HUD
    // contains the original gameplay/pause sprites plus only the glyphs that
    // were emitted by that extraction pass. Keep the general DPUI validator
    // strict; accept this legacy shape only at the canonical gameplay boundary
    // after independently validating its complete binary structure and CRC.
    playable::PackageView compact = {};
    const playable::PackageError base = playable::validate_package(
        source, size, "DPUI", &compact);
    if (base != playable::PackageError::Ok) {
        return base;
    }

    const std::uint8_t* bytes = compact.bytes;
    if (playable::read_u16(bytes + 4) != 2 ||
        playable::read_u32(bytes + 16) != 512 ||
        playable::read_u32(bytes + 20) != 128 ||
        playable::read_u32(bytes + 24) != 2 ||
        playable::read_u32(bytes + 52) != 604 ||
        playable::read_u32(bytes + 56) != 448 ||
        playable::read_u32(bytes + 60) == 0 ||
        playable::read_u32(bytes + 64) != 1) {
        return playable::PackageError::Range;
    }

    const std::uint32_t quads = playable::read_u32(bytes + 28);
    const std::uint32_t table = playable::read_u32(bytes + 32);
    const std::uint32_t stride = playable::read_u32(bytes + 36);
    const std::uint32_t atlas = playable::read_u32(bytes + 40);
    const std::uint32_t atlas_bytes = playable::read_u32(bytes + 44);
    if (quads == 0 || quads > 128 || stride != 32 ||
        !package_range(table, quads, stride, size) ||
        atlas > size || atlas_bytes != 512u * 128u * 2u ||
        atlas_bytes > size - atlas || atlas_bytes > 196608) {
        return playable::PackageError::Range;
    }

    bool identities[384] = {};
    for (std::uint32_t index = 0; index < quads; ++index) {
        const std::uint8_t* item = bytes + table + index * stride;
        const std::uint16_t id = playable::read_u16(item);
        const std::uint32_t u = playable::read_u16(item + 12);
        const std::uint32_t v = playable::read_u16(item + 14);
        const std::uint32_t width = playable::read_u16(item + 16);
        const std::uint32_t height = playable::read_u16(item + 18);
        if (id >= 384 || identities[id] || width == 0 || height == 0 ||
            u > 512 || width > 512 - u ||
            v > 128 || height > 128 - v ||
            playable::read_u32(item + 24) == 0 ||
            (id >= 128 && playable::read_u16(item + 28) == 0)) {
            return playable::PackageError::Range;
        }
        identities[id] = true;
    }

    constexpr std::uint16_t required_sprites[] = {
        0, 1, 2, 3,
        10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
        20, 30, 40, 41, 42, 43,
    };
    for (const std::uint16_t id : required_sprites) {
        if (!identities[id]) {
            return playable::PackageError::Missing;
        }
    }

    *view = compact;
    return playable::PackageError::Ok;
}

CanonicalCommonLoadError load_one(
    const char* relative_path,
    Validator validator,
    CanonicalCommonLoadError validation_error,
    OwnedPlayablePackage* package) {
    char path[256] = {};
    if (!make_game_relative_path(relative_path, path, sizeof(path))) {
        return CanonicalCommonLoadError::Path;
    }

    std::uint32_t size = 0;
    if (!file_size(path, &size)) {
        return CanonicalCommonLoadError::FileSize;
    }

    auto* bytes = static_cast<std::uint8_t*>(std::malloc(size));
    if (bytes == nullptr) {
        return CanonicalCommonLoadError::Allocation;
    }

    std::uint32_t read = 0;
    if (!read_file(path, bytes, size, &read) || read != size) {
        std::free(bytes);
        return CanonicalCommonLoadError::Read;
    }

    playable::PackageView view = {};
    if (validator(bytes, size, &view) != playable::PackageError::Ok) {
        std::free(bytes);
        return validation_error;
    }

    package->bytes = bytes;
    package->size = size;
    package->view = view;
    return CanonicalCommonLoadError::Ok;
}

void unload_one(OwnedPlayablePackage* package) {
    if (package == nullptr) {
        return;
    }
    std::free(package->bytes);
    *package = {};
}

}  // namespace

CanonicalCommonLoadError load_canonical_common_packages(
    const CanonicalCommonAssets& assets,
    CanonicalCommonPackages* packages) {
    if (packages == nullptr) {
        return CanonicalCommonLoadError::InvalidOutput;
    }
    *packages = {};

    CanonicalCommonLoadError error = load_one(
        assets.link_model, playable::validate_dpsk,
        CanonicalCommonLoadError::LinkModelPackage, &packages->link_model);
    if (error != CanonicalCommonLoadError::Ok) {
        return error;
    }
    error = load_one(
        assets.link_textures, playable::validate_dptx,
        CanonicalCommonLoadError::LinkTexturePackage, &packages->link_textures);
    if (error != CanonicalCommonLoadError::Ok) {
        unload_canonical_common_packages(packages);
        return error;
    }
    error = load_one(
        assets.link_animations, playable::validate_dpan,
        CanonicalCommonLoadError::LinkAnimationPackage,
        &packages->link_animations);
    if (error != CanonicalCommonLoadError::Ok) {
        unload_canonical_common_packages(packages);
        return error;
    }
    error = load_one(
        assets.hud_ui, validate_canonical_hud_dpui,
        CanonicalCommonLoadError::HudPackage, &packages->hud_ui);
    if (error != CanonicalCommonLoadError::Ok) {
        unload_canonical_common_packages(packages);
        return error;
    }
    return CanonicalCommonLoadError::Ok;
}

void unload_canonical_common_packages(CanonicalCommonPackages* packages) {
    if (packages == nullptr) {
        return;
    }
    unload_one(&packages->link_model);
    unload_one(&packages->link_textures);
    unload_one(&packages->link_animations);
    unload_one(&packages->hud_ui);
}

playable::PackageSet canonical_playable_package_set(
    const CanonicalCommonPackages& packages) {
    return {
        packages.link_model.view,
        packages.link_textures.view,
        packages.link_animations.view,
        packages.hud_ui.view,
    };
}

const char* canonical_common_load_error_name(CanonicalCommonLoadError error) {
    switch (error) {
    case CanonicalCommonLoadError::Ok: return "ok";
    case CanonicalCommonLoadError::InvalidOutput: return "invalid_output";
    case CanonicalCommonLoadError::Path: return "path";
    case CanonicalCommonLoadError::FileSize: return "file_size";
    case CanonicalCommonLoadError::Allocation: return "allocation";
    case CanonicalCommonLoadError::Read: return "read";
    case CanonicalCommonLoadError::LinkModelPackage: return "link_model_package";
    case CanonicalCommonLoadError::LinkTexturePackage: return "link_texture_package";
    case CanonicalCommonLoadError::LinkAnimationPackage: return "link_animation_package";
    case CanonicalCommonLoadError::HudPackage: return "hud_package";
    }
    return "unknown";
}

}  // namespace dusk::psp::game
