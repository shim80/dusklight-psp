#include "dusk/psp/canonical_common_loader.hpp"

#include "dusk/psp/platform.hpp"

#include <cstdlib>

namespace dusk::psp::game {
namespace {

using Validator = playable::PackageError (*)(
    const void*, std::uint32_t, playable::PackageView*);

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
        assets.hud_ui, playable::validate_dpui,
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
