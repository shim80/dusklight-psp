#include "dusk/psp/canonical_room_loader.hpp"

#include "dusk/psp/platform.hpp"

#include <cstdlib>

namespace dusk::psp::game {
namespace {

using Validator = room::PackageError (*)(const void*, std::uint32_t, room::PackageView*);

CanonicalRoomLoadError load_one(
    const char* relative_path,
    Validator validator,
    CanonicalRoomLoadError validation_error,
    OwnedPackage* package) {
    char path[256] = {};
    if (!make_game_relative_path(relative_path, path, sizeof(path))) {
        return CanonicalRoomLoadError::Path;
    }

    std::uint32_t size = 0;
    if (!file_size(path, &size)) {
        return CanonicalRoomLoadError::FileSize;
    }

    auto* bytes = static_cast<std::uint8_t*>(std::malloc(size));
    if (bytes == nullptr) {
        return CanonicalRoomLoadError::Allocation;
    }

    std::uint32_t read = 0;
    if (!read_file(path, bytes, size, &read) || read != size) {
        std::free(bytes);
        return CanonicalRoomLoadError::Read;
    }

    room::PackageView view = {};
    if (validator(bytes, size, &view) != room::PackageError::Ok) {
        std::free(bytes);
        return validation_error;
    }

    package->bytes = bytes;
    package->size = size;
    package->view = view;
    return CanonicalRoomLoadError::Ok;
}

void unload_one(OwnedPackage* package) {
    if (package == nullptr) {
        return;
    }
    std::free(package->bytes);
    *package = {};
}

}  // namespace

CanonicalRoomLoadError load_canonical_room_packages(
    const CanonicalRoomAssets& assets,
    CanonicalRoomPackages* packages) {
    if (packages == nullptr) {
        return CanonicalRoomLoadError::InvalidOutput;
    }
    *packages = {};

    CanonicalRoomLoadError error = load_one(
        assets.model, room::validate_dprm,
        CanonicalRoomLoadError::ModelPackage, &packages->model);
    if (error != CanonicalRoomLoadError::Ok) {
        return error;
    }
    error = load_one(
        assets.textures, room::validate_room_dptx,
        CanonicalRoomLoadError::TexturePackage, &packages->textures);
    if (error != CanonicalRoomLoadError::Ok) {
        unload_canonical_room_packages(packages);
        return error;
    }
    error = load_one(
        assets.collision, room::validate_dpcl,
        CanonicalRoomLoadError::CollisionPackage, &packages->collision);
    if (error != CanonicalRoomLoadError::Ok) {
        unload_canonical_room_packages(packages);
        return error;
    }
    error = load_one(
        assets.scene, room::validate_dpsc,
        CanonicalRoomLoadError::ScenePackage, &packages->scene);
    if (error != CanonicalRoomLoadError::Ok) {
        unload_canonical_room_packages(packages);
        return error;
    }
    return CanonicalRoomLoadError::Ok;
}

void unload_canonical_room_packages(CanonicalRoomPackages* packages) {
    if (packages == nullptr) {
        return;
    }
    unload_one(&packages->model);
    unload_one(&packages->textures);
    unload_one(&packages->collision);
    unload_one(&packages->scene);
}

const char* canonical_room_load_error_name(CanonicalRoomLoadError error) {
    switch (error) {
    case CanonicalRoomLoadError::Ok: return "ok";
    case CanonicalRoomLoadError::InvalidOutput: return "invalid_output";
    case CanonicalRoomLoadError::Path: return "path";
    case CanonicalRoomLoadError::FileSize: return "file_size";
    case CanonicalRoomLoadError::Allocation: return "allocation";
    case CanonicalRoomLoadError::Read: return "read";
    case CanonicalRoomLoadError::ModelPackage: return "model_package";
    case CanonicalRoomLoadError::TexturePackage: return "texture_package";
    case CanonicalRoomLoadError::CollisionPackage: return "collision_package";
    case CanonicalRoomLoadError::ScenePackage: return "scene_package";
    }
    return "unknown";
}

}  // namespace dusk::psp::game
