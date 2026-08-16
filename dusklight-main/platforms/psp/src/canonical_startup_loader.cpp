#include "dusk/psp/canonical_startup_loader.hpp"

#include "dusk/psp/platform.hpp"

#include <cstdlib>

namespace dusk::psp::game {
namespace {

struct RawPackage {
    std::uint8_t* bytes;
    std::uint32_t size;
};

CanonicalStartupLoadError load_raw(
    const char* relative_path,
    RawPackage* output) {
    if (relative_path == nullptr || output == nullptr) {
        return CanonicalStartupLoadError::AssetContract;
    }
    *output = {};
    char path[256] = {};
    if (!make_game_relative_path(relative_path, path, sizeof(path))) {
        return CanonicalStartupLoadError::Path;
    }
    std::uint32_t size = 0;
    if (!file_size(path, &size) || size == 0) {
        return CanonicalStartupLoadError::FileSize;
    }
    auto* bytes = static_cast<std::uint8_t*>(std::malloc(size));
    if (bytes == nullptr) {
        return CanonicalStartupLoadError::Allocation;
    }
    std::uint32_t read = 0;
    if (!read_file(path, bytes, size, &read) || read != size) {
        std::free(bytes);
        return CanonicalStartupLoadError::Read;
    }
    output->bytes = bytes;
    output->size = size;
    return CanonicalStartupLoadError::Ok;
}

void unload_raw(std::uint8_t** bytes) {
    if (bytes == nullptr) {
        return;
    }
    std::free(*bytes);
    *bytes = nullptr;
}

CanonicalStartupLoadError load_sequence(
    const char* path,
    OwnedStartupSequence* output) {
    RawPackage raw = {};
    CanonicalStartupLoadError error = load_raw(path, &raw);
    if (error != CanonicalStartupLoadError::Ok) {
        return error;
    }
    startup::PackageView view = {};
    if (startup::validate_startup_package(raw.bytes, raw.size, &view) !=
        startup::PackageError::Ok) {
        std::free(raw.bytes);
        return CanonicalStartupLoadError::SequencePackage;
    }
    *output = {raw.bytes, raw.size, view};
    return CanonicalStartupLoadError::Ok;
}

CanonicalStartupLoadError load_ui(
    const char* path,
    CanonicalStartupLoadError package_error,
    OwnedStartupUi* output) {
    RawPackage raw = {};
    CanonicalStartupLoadError error = load_raw(path, &raw);
    if (error != CanonicalStartupLoadError::Ok) {
        return error;
    }
    startup::UiPackageView view = {};
    if (startup::validate_startup_ui(raw.bytes, raw.size, &view) !=
        startup::UiPackageError::Ok) {
        std::free(raw.bytes);
        return package_error;
    }
    *output = {raw.bytes, raw.size, view};
    return CanonicalStartupLoadError::Ok;
}

using RoomValidator = room::PackageError (*)(
    const void*, std::uint32_t, room::PackageView*);

CanonicalStartupLoadError load_room_package(
    const char* path,
    RoomValidator validator,
    CanonicalStartupLoadError package_error,
    OwnedStartupRoomPackage* output) {
    RawPackage raw = {};
    CanonicalStartupLoadError error = load_raw(path, &raw);
    if (error != CanonicalStartupLoadError::Ok) {
        return error;
    }
    room::PackageView view = {};
    if (validator(raw.bytes, raw.size, &view) != room::PackageError::Ok) {
        std::free(raw.bytes);
        return package_error;
    }
    *output = {raw.bytes, raw.size, view};
    return CanonicalStartupLoadError::Ok;
}

CanonicalStartupLoadError load_animation(
    const char* path,
    OwnedStartupAnimation* output) {
    RawPackage raw = {};
    CanonicalStartupLoadError error = load_raw(path, &raw);
    if (error != CanonicalStartupLoadError::Ok) {
        return error;
    }
    playable::PackageView view = {};
    if (playable::validate_dpan(raw.bytes, raw.size, &view) !=
        playable::PackageError::Ok) {
        std::free(raw.bytes);
        return CanonicalStartupLoadError::TitleLogoAnimationPackage;
    }
    *output = {raw.bytes, raw.size, view};
    return CanonicalStartupLoadError::Ok;
}

}  // namespace

CanonicalStartupLoadError load_canonical_startup_packages(
    CanonicalStartupPackages* packages) {
    if (packages == nullptr) {
        return CanonicalStartupLoadError::InvalidOutput;
    }
    *packages = {};
    CanonicalStartupAssets assets = {};
    if (resolve_canonical_startup_assets(&assets) != CanonicalAssetError::Ok) {
        return CanonicalStartupLoadError::AssetContract;
    }

    CanonicalStartupLoadError error =
        load_sequence(assets.sequence, &packages->sequence);
    if (error != CanonicalStartupLoadError::Ok) {
        return error;
    }
    error = load_ui(
        assets.logos_ui, CanonicalStartupLoadError::LogosUiPackage,
        &packages->logos_ui);
    if (error != CanonicalStartupLoadError::Ok) {
        unload_canonical_startup_packages(packages);
        return error;
    }
    error = load_ui(
        assets.title_ui, CanonicalStartupLoadError::TitleUiPackage,
        &packages->title_ui);
    if (error != CanonicalStartupLoadError::Ok) {
        unload_canonical_startup_packages(packages);
        return error;
    }
    error = load_ui(
        assets.file_select_ui, CanonicalStartupLoadError::FileSelectUiPackage,
        &packages->file_select_ui);
    if (error != CanonicalStartupLoadError::Ok) {
        unload_canonical_startup_packages(packages);
        return error;
    }
    error = load_room_package(
        assets.title_room_model, room::validate_dprm,
        CanonicalStartupLoadError::TitleRoomModelPackage,
        &packages->title_room_model);
    if (error != CanonicalStartupLoadError::Ok) {
        unload_canonical_startup_packages(packages);
        return error;
    }
    error = load_room_package(
        assets.title_room_textures, room::validate_room_dptx,
        CanonicalStartupLoadError::TitleRoomTexturePackage,
        &packages->title_room_textures);
    if (error != CanonicalStartupLoadError::Ok) {
        unload_canonical_startup_packages(packages);
        return error;
    }
    error = load_room_package(
        assets.title_logo_model, room::validate_dprm,
        CanonicalStartupLoadError::TitleLogoModelPackage,
        &packages->title_logo_model);
    if (error != CanonicalStartupLoadError::Ok) {
        unload_canonical_startup_packages(packages);
        return error;
    }
    error = load_room_package(
        assets.title_logo_textures, room::validate_room_dptx,
        CanonicalStartupLoadError::TitleLogoTexturePackage,
        &packages->title_logo_textures);
    if (error != CanonicalStartupLoadError::Ok) {
        unload_canonical_startup_packages(packages);
        return error;
    }
    error = load_animation(
        assets.title_logo_animation, &packages->title_logo_animation);
    if (error != CanonicalStartupLoadError::Ok) {
        unload_canonical_startup_packages(packages);
        return error;
    }
    return CanonicalStartupLoadError::Ok;
}

CanonicalStartupLoadError load_canonical_file_select_ui(
    OwnedStartupUi* package) {
    if (package == nullptr) {
        return CanonicalStartupLoadError::InvalidOutput;
    }
    *package = {};
    CanonicalStartupAssets assets = {};
    if (resolve_canonical_startup_assets(&assets) != CanonicalAssetError::Ok) {
        return CanonicalStartupLoadError::AssetContract;
    }
    return load_ui(
        assets.file_select_ui,
        CanonicalStartupLoadError::FileSelectUiPackage, package);
}

void unload_canonical_startup_ui(OwnedStartupUi* package) {
    if (package == nullptr) {
        return;
    }
    unload_raw(&package->bytes);
    *package = {};
}

void unload_canonical_startup_packages(CanonicalStartupPackages* packages) {
    if (packages == nullptr) {
        return;
    }
    unload_raw(&packages->sequence.bytes);
    unload_raw(&packages->logos_ui.bytes);
    unload_raw(&packages->title_ui.bytes);
    unload_raw(&packages->file_select_ui.bytes);
    unload_raw(&packages->title_room_model.bytes);
    unload_raw(&packages->title_room_textures.bytes);
    unload_raw(&packages->title_logo_model.bytes);
    unload_raw(&packages->title_logo_textures.bytes);
    unload_raw(&packages->title_logo_animation.bytes);
    *packages = {};
}

const char* canonical_startup_load_error_name(CanonicalStartupLoadError error) {
    switch (error) {
    case CanonicalStartupLoadError::Ok: return "ok";
    case CanonicalStartupLoadError::InvalidOutput: return "invalid_output";
    case CanonicalStartupLoadError::AssetContract: return "asset_contract";
    case CanonicalStartupLoadError::Path: return "path";
    case CanonicalStartupLoadError::FileSize: return "file_size";
    case CanonicalStartupLoadError::Allocation: return "allocation";
    case CanonicalStartupLoadError::Read: return "read";
    case CanonicalStartupLoadError::SequencePackage: return "startup_dpst";
    case CanonicalStartupLoadError::LogosUiPackage: return "startup_logos_dpsu";
    case CanonicalStartupLoadError::TitleUiPackage: return "title_ui_dpsu";
    case CanonicalStartupLoadError::FileSelectUiPackage: return "file_select_dpsu";
    case CanonicalStartupLoadError::TitleRoomModelPackage: return "title_room_dprm";
    case CanonicalStartupLoadError::TitleRoomTexturePackage: return "title_room_dptx";
    case CanonicalStartupLoadError::TitleLogoModelPackage: return "title_logo_dprm";
    case CanonicalStartupLoadError::TitleLogoTexturePackage: return "title_logo_dptx";
    case CanonicalStartupLoadError::TitleLogoAnimationPackage: return "title_logo_dpan";
    }
    return "unknown";
}

}  // namespace dusk::psp::game
