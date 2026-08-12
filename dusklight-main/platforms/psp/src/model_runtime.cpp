#include "dusk/psp/model_runtime.hpp"

#include "d/d_com_inf_game.h"
#include "dusk/psp/playable_package.hpp"
#include "dusk/psp/process_runtime.hpp"
#include "dusk/psp/room_package.hpp"

#include <algorithm>
#include <cstring>

namespace {

struct PackageDescriptor {
    const char* archive;
    int resource_id;
    const char* model_resource;
    const char* texture_resource;
    int animation_resource_id;
    const char* animation_resource;
    int collision_resource_id;
    const char* collision_resource;
    int secondary_collision_resource_id;
    const char* secondary_collision_resource;
};

constexpr PackageDescriptor kPackages[] = {
    {
        "L4HsMato", 4, "object:L4HsMato:4:model",
        "object:L4HsMato:4:textures",
        -1, nullptr,
        7, "object:L4HsMato:7:collision",
        -1, nullptr,
    },
    {
        "P_Gear", 4, "object:P_Gear:4:model",
        "object:P_Gear:4:textures",
        -1, nullptr,
        -1, nullptr,
        -1, nullptr,
    },
    {
        "P_Gear", 3, "object:P_Gear:3:model",
        "object:P_Gear:3:textures",
        -1, nullptr,
        -1, nullptr,
        -1, nullptr,
    },
    {
        "L4R02Gate", 4, "object:L4R02Gate:4:model",
        "object:L4R02Gate:4:textures",
        -1, nullptr,
        7, "object:L4R02Gate:7:collision",
        -1, nullptr,
    },
    {
        "P_Sswitch", 4, "object:P_Sswitch:4:model",
        "object:P_Sswitch:4:textures",
        -1, nullptr,
        9, "object:P_Sswitch:9:collision",
        -1, nullptr,
    },
    {
        "P_Sswitch", 5, "object:P_Sswitch:5:model",
        "object:P_Sswitch:5:textures",
        -1, nullptr,
        8, "object:P_Sswitch:8:collision",
        -1, nullptr,
    },
    {
        "Dalways", 13, "object:Dalways:13:model",
        "object:Dalways:13:textures",
        8, "object:Dalways:8:animation",
        27, "object:Dalways:27:collision",
        28, "object:Dalways:28:collision",
    },
};

dusk::psp::model::PspStaticModelRuntime* g_runtime = nullptr;

bool archive_matches(const char* archive) {
    if (archive == nullptr) {
        return false;
    }
    for (const auto& package : kPackages) {
        if (std::strcmp(archive, package.archive) == 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

namespace dusk::psp::model {

bool PspStaticModelRuntime::initialize(
    resources::PspResourceManager* resources,
    render::PspRenderQueue* render_queue,
    movebg::PspMoveBgWorld* movebg_world) {
    shutdown();
    if (resources == nullptr || render_queue == nullptr ||
        !resources->initialized() || !render_queue->initialized()) {
        ++metrics.errors;
        return false;
    }
    resources_ = resources;
    render_queue_ = render_queue;
    movebg_world_ = movebg_world;
    metrics = {};
    initialized_ = true;
    return true;
}

void PspStaticModelRuntime::shutdown() {
    if (movebg_world_ != nullptr) {
        for (auto& slot : movebg_slots_) {
            if (slot.actor != nullptr && slot.collision_active) {
                movebg_world_->destroy(slot.handle);
            }
        }
    }
    for (auto& slot : movebg_slots_) {
        slot = {};
    }
    for (dBgW*& collision : direct_bg_slots_) {
        if (collision != nullptr && collision->used_ &&
            movebg_world_ != nullptr) {
            movebg_world_->destroy(collision->handle_);
        }
        if (collision != nullptr) {
            collision->used_ = false;
        }
        collision = nullptr;
    }
    for (auto& model : models_) {
        model.active_ = false;
        model.owner_ = nullptr;
        model.data_ = nullptr;
    }
    for (auto& arena : arenas_) {
        arena = PspActorHeapArena{};
    }
    if (resources_ != nullptr) {
        for (auto& package : packages_) {
            if (package.loaded) {
                if (package.collision_size != 0) {
                    resources_->release(package.collision_handle);
                }
                if (package.secondary_collision_size != 0) {
                    resources_->release(
                        package.secondary_collision_handle);
                }
                if (package.animation_size != 0) {
                    resources_->release(package.animation_handle);
                }
                resources_->release(package.texture_handle);
                resources_->release(package.model_handle);
            }
        }
    }
    resources_ = nullptr;
    render_queue_ = nullptr;
    movebg_world_ = nullptr;
    for (auto& package : packages_) {
        clear_package(package);
    }
    references_ = 0;
    initialized_ = false;
}

bool PspStaticModelRuntime::load_archive(const char* archive) {
    bool found = false;
    for (std::uint16_t index = 0; index < kPackageCapacity; ++index) {
        if (std::strcmp(archive, kPackages[index].archive) != 0) {
            continue;
        }
        found = true;
        PackageSlot& package = packages_[index];
        if (package.loaded) {
            continue;
        }
        room::PackageView model_view = {};
        room::PackageView texture_view = {};
        if (!resources_->load(
                kPackages[index].model_resource,
                resources::PspResourceType::StaticModel,
                package.model_bytes, sizeof(package.model_bytes),
                &package.model_handle, &package.model_size) ||
            room::validate_dprm(
                package.model_bytes, package.model_size, &model_view) !=
                room::PackageError::Ok) {
            if (package.model_size != 0) {
                resources_->release(package.model_handle);
            }
            clear_package(package);
            ++metrics.errors;
            release_archive(archive);
            return false;
        }
        if (!resources_->load(
                kPackages[index].texture_resource,
                resources::PspResourceType::TextureArchive,
                package.texture_bytes, sizeof(package.texture_bytes),
                &package.texture_handle, &package.texture_size) ||
            room::validate_room_dptx(
                package.texture_bytes, package.texture_size,
                &texture_view) != room::PackageError::Ok) {
            resources_->release(package.model_handle);
            package.model_handle = {};
            ++metrics.errors;
            release_archive(archive);
            return false;
        }
        if (kPackages[index].collision_resource != nullptr) {
            room::PackageView collision_view = {};
            if (!resources_->load(
                    kPackages[index].collision_resource,
                    resources::PspResourceType::RoomCollision,
                    package.collision_bytes,
                    sizeof(package.collision_bytes),
                    &package.collision_handle,
                    &package.collision_size) ||
                room::validate_dpcl(
                    package.collision_bytes,
                    package.collision_size,
                    &collision_view) != room::PackageError::Ok) {
                if (package.collision_size != 0) {
                    resources_->release(package.collision_handle);
                }
                resources_->release(package.texture_handle);
                resources_->release(package.model_handle);
                clear_package(package);
                ++metrics.errors;
                release_archive(archive);
                return false;
            }
        }
        if (kPackages[index].secondary_collision_resource != nullptr) {
            room::PackageView collision_view = {};
            if (!resources_->load(
                    kPackages[index].secondary_collision_resource,
                    resources::PspResourceType::RoomCollision,
                    package.secondary_collision_bytes,
                    sizeof(package.secondary_collision_bytes),
                    &package.secondary_collision_handle,
                    &package.secondary_collision_size) ||
                room::validate_dpcl(
                    package.secondary_collision_bytes,
                    package.secondary_collision_size,
                    &collision_view) != room::PackageError::Ok) {
                if (package.secondary_collision_size != 0) {
                    resources_->release(
                        package.secondary_collision_handle);
                }
                if (package.collision_size != 0) {
                    resources_->release(package.collision_handle);
                }
                resources_->release(package.texture_handle);
                resources_->release(package.model_handle);
                clear_package(package);
                ++metrics.errors;
                release_archive(archive);
                return false;
            }
        }
        if (kPackages[index].animation_resource != nullptr) {
            playable::PackageView animation_view = {};
            if (!resources_->load(
                    kPackages[index].animation_resource,
                    resources::PspResourceType::AnimationArchive,
                    package.animation_bytes,
                    sizeof(package.animation_bytes),
                    &package.animation_handle,
                    &package.animation_size) ||
                playable::validate_dpan(
                    package.animation_bytes,
                    package.animation_size,
                    &animation_view) != playable::PackageError::Ok ||
                !package.animation.configure(
                    animation_view,
                    static_cast<std::uint32_t>(
                        kPackages[index].animation_resource_id))) {
                if (package.animation_size != 0) {
                    resources_->release(package.animation_handle);
                }
                if (package.secondary_collision_size != 0) {
                    resources_->release(
                        package.secondary_collision_handle);
                }
                if (package.collision_size != 0) {
                    resources_->release(package.collision_handle);
                }
                resources_->release(package.texture_handle);
                resources_->release(package.model_handle);
                clear_package(package);
                ++metrics.errors;
                release_archive(archive);
                return false;
            }
        }
        package.model_data.model_bytes_ = model_view.bytes;
        package.model_data.model_size_ = model_view.size;
        package.model_data.texture_bytes_ = texture_view.bytes;
        package.model_data.texture_size_ = texture_view.size;
        metrics.runtime_material_count +=
            room::read_u32(model_view.bytes + 36);
        metrics.runtime_texture_count +=
            room::read_u32(texture_view.bytes + 16);
        metrics.runtime_texture_bytes +=
            room::read_u32(texture_view.bytes + 48);
        package.loaded = true;
    }
    return found;
}

void PspStaticModelRuntime::release_archive(const char* archive) {
    if (resources_ == nullptr || archive == nullptr) {
        return;
    }
    for (std::uint16_t index = 0; index < kPackageCapacity; ++index) {
        PackageSlot& package = packages_[index];
        if (package.loaded && package.references == 0 &&
            std::strcmp(archive, kPackages[index].archive) == 0) {
            resources_->release(package.texture_handle);
            resources_->release(package.model_handle);
            if (package.collision_size != 0) {
                resources_->release(package.collision_handle);
            }
            if (package.secondary_collision_size != 0) {
                resources_->release(
                    package.secondary_collision_handle);
            }
            if (package.animation_size != 0) {
                resources_->release(package.animation_handle);
            }
            clear_package(package);
        }
    }
}

void PspStaticModelRuntime::clear_package(PackageSlot& package) {
    package.model_handle = {};
    package.texture_handle = {};
    package.animation_handle = {};
    package.collision_handle = {};
    package.secondary_collision_handle = {};
    package.model_size = 0;
    package.texture_size = 0;
    package.animation_size = 0;
    package.collision_size = 0;
    package.secondary_collision_size = 0;
    package.references = 0;
    package.model_data = {};
    package.animation = {};
    package.loaded = false;
}

PspStaticModelRuntime::PackageSlot*
PspStaticModelRuntime::find_package(
    const char* archive, int resource_id) {
    if (archive == nullptr) {
        return nullptr;
    }
    for (std::uint16_t index = 0; index < kPackageCapacity; ++index) {
        if (kPackages[index].resource_id == resource_id &&
            std::strcmp(archive, kPackages[index].archive) == 0) {
            return &packages_[index];
        }
    }
    return nullptr;
}

const PspStaticModelRuntime::PackageSlot*
PspStaticModelRuntime::find_package(
    const char* archive, int resource_id) const {
    return const_cast<PspStaticModelRuntime*>(this)->find_package(
        archive, resource_id);
}

PspStaticModelRuntime::PackageSlot*
PspStaticModelRuntime::find_package(J3DModelData* data) {
    for (auto& package : packages_) {
        if (package.loaded && &package.model_data == data) {
            return &package;
        }
    }
    return nullptr;
}

PspStaticModelRuntime::PackageSlot*
PspStaticModelRuntime::find_archive_package(const char* archive) {
    if (archive == nullptr) {
        return nullptr;
    }
    for (std::uint16_t index = 0; index < kPackageCapacity; ++index) {
        if (packages_[index].loaded &&
            std::strcmp(archive, kPackages[index].archive) == 0) {
            return &packages_[index];
        }
    }
    return nullptr;
}

PspStaticModelRuntime::PackageSlot*
PspStaticModelRuntime::find_collision_package(
    const char* archive, int resource_id) {
    if (archive == nullptr) {
        return nullptr;
    }
    for (std::uint16_t index = 0; index < kPackageCapacity; ++index) {
        if (kPackages[index].collision_resource_id == resource_id &&
            std::strcmp(archive, kPackages[index].archive) == 0) {
            return &packages_[index];
        }
        if (kPackages[index].secondary_collision_resource_id ==
                resource_id &&
            std::strcmp(archive, kPackages[index].archive) == 0) {
            return &packages_[index];
        }
    }
    return nullptr;
}

const void* PspStaticModelRuntime::collision_data(
    const PackageSlot& package, const char* archive,
    int resource_id, std::uint32_t* size) const {
    if (archive == nullptr || size == nullptr) {
        return nullptr;
    }
    for (std::uint16_t index = 0; index < kPackageCapacity; ++index) {
        if (&packages_[index] != &package ||
            std::strcmp(archive, kPackages[index].archive) != 0) {
            continue;
        }
        if (resource_id == kPackages[index].collision_resource_id) {
            *size = package.collision_size;
            return package.collision_bytes;
        }
        if (resource_id ==
            kPackages[index].secondary_collision_resource_id) {
            *size = package.secondary_collision_size;
            return package.secondary_collision_bytes;
        }
    }
    return nullptr;
}

PspStaticModelRuntime::MoveBgSlot*
PspStaticModelRuntime::find_movebg(dBgS_MoveBgActor* actor) {
    for (auto& slot : movebg_slots_) {
        if (slot.actor == actor) {
            return &slot;
        }
    }
    return nullptr;
}

const PspStaticModelRuntime::MoveBgSlot*
PspStaticModelRuntime::find_movebg(
    const dBgS_MoveBgActor* actor) const {
    return const_cast<PspStaticModelRuntime*>(this)->find_movebg(
        const_cast<dBgS_MoveBgActor*>(actor));
}

int PspStaticModelRuntime::res_load(
    request_of_phase_process_class* phase, const char* archive) {
    ++metrics.archive_requests;
    if (!initialized_ || phase == nullptr || !archive_matches(archive)) {
        ++metrics.errors;
        return cPhs_ERROR_e;
    }
    if (phase->state == cPhs_INIT_e) {
        if (!load_archive(archive)) {
            phase->state = cPhs_ERROR_e;
            return cPhs_ERROR_e;
        }
        phase->state = cPhs_LOADING_e;
        phase->generation =
            1;
        return cPhs_LOADING_e;
    }
    if (phase->state == cPhs_LOADING_e) {
        phase->state = cPhs_COMPLEATE_e;
        for (std::uint16_t index = 0; index < kPackageCapacity; ++index) {
            if (packages_[index].loaded &&
                std::strcmp(archive, kPackages[index].archive) == 0) {
                ++packages_[index].references;
            }
        }
        ++references_;
        return cPhs_COMPLEATE_e;
    }
    if (phase->state == cPhs_COMPLEATE_e) {
        return cPhs_COMPLEATE_e;
    }
    ++metrics.errors;
    phase->state = cPhs_ERROR_e;
    return cPhs_ERROR_e;
}

int PspStaticModelRuntime::res_delete(
    request_of_phase_process_class* phase, const char* archive) {
    ++metrics.resource_release_calls;
    if (!initialized_ || phase == nullptr || !archive_matches(archive) ||
        phase->state != cPhs_COMPLEATE_e || references_ == 0) {
        ++metrics.errors;
        return 0;
    }
    --references_;
    for (std::uint16_t index = 0; index < kPackageCapacity; ++index) {
        if (packages_[index].loaded &&
            std::strcmp(archive, kPackages[index].archive) == 0 &&
            packages_[index].references != 0) {
            --packages_[index].references;
        }
    }
    phase->state = cPhs_INIT_e;
    phase->generation = 0;
    if (void* owner = process::current_instance()) {
        destroy_owner(owner);
        if (PspActorHeapArena* arena = find_arena(owner)) {
            arena->release(owner);
        }
    }
    release_archive(archive);
    return 1;
}

void* PspStaticModelRuntime::object_resource(
    const char* archive, int resource_id) {
    ++metrics.resource_get_calls;
    PackageSlot* package = find_package(archive, resource_id);
    if (package == nullptr) {
        package = find_collision_package(archive, resource_id);
        if (initialized_ && package != nullptr && package->loaded) {
            std::uint32_t size = 0;
            return const_cast<void*>(
                collision_data(
                    *package, archive, resource_id, &size));
        }
    }
    if (!initialized_ || package == nullptr || !package->loaded) {
        for (std::uint16_t index = 0; index < kPackageCapacity; ++index) {
            if (kPackages[index].animation_resource_id == resource_id &&
                std::strcmp(archive, kPackages[index].archive) == 0 &&
                packages_[index].loaded &&
                packages_[index].animation_size != 0) {
                return &packages_[index].animation;
            }
        }
        ++metrics.errors;
        return nullptr;
    }
    return &package->model_data;
}

bool PspStaticModelRuntime::direct_bg_set(
    dBgW* collision, const void* data, Mtx* matrix) {
    if (!initialized_ || collision == nullptr || data == nullptr ||
        matrix == nullptr || collision->configured_) {
        ++metrics.errors;
        return false;
    }
    std::uint32_t collision_size = 0;
    for (auto& candidate : packages_) {
        if (candidate.loaded && candidate.collision_size != 0 &&
            candidate.collision_bytes == data) {
            collision_size = candidate.collision_size;
            break;
        }
        if (candidate.loaded &&
            candidate.secondary_collision_size != 0 &&
            candidate.secondary_collision_bytes == data) {
            collision_size = candidate.secondary_collision_size;
            break;
        }
    }
    if (collision_size == 0) {
        ++metrics.errors;
        return false;
    }
    collision->collision_ = data;
    collision->collision_size_ = collision_size;
    collision->matrix_ = matrix;
    collision->handle_ = {};
    collision->used_ = false;
    collision->configured_ = true;
    return true;
}

bool PspStaticModelRuntime::direct_bg_register(dBgW* collision) {
    if (!initialized_ || movebg_world_ == nullptr ||
        collision == nullptr || !collision->configured_) {
        ++metrics.errors;
        return false;
    }
    for (auto& binding : movebg_slots_) {
        if (binding.actor != nullptr &&
            collision == &binding.primary_collision) {
            collision->used_ = true;
            return true;
        }
    }
    if (collision->used_) {
        ++metrics.errors;
        return false;
    }
    dBgW** slot = nullptr;
    for (dBgW*& candidate : direct_bg_slots_) {
        if (candidate == nullptr) {
            slot = &candidate;
            break;
        }
    }
    movebg::Matrix34 matrix = {};
    std::memcpy(&matrix, collision->matrix_, sizeof(matrix));
    if (slot == nullptr ||
        !movebg_world_->create(
            collision->collision_, collision->collision_size_,
            matrix, &collision->handle_)) {
        ++metrics.errors;
        return false;
    }
    *slot = collision;
    collision->used_ = true;
    ++metrics.movebg_creates;
    return true;
}

bool PspStaticModelRuntime::direct_bg_move(dBgW* collision) {
    if (!initialized_ || movebg_world_ == nullptr ||
        collision == nullptr || !collision->configured_) {
        ++metrics.errors;
        return false;
    }
    for (auto& binding : movebg_slots_) {
        if (binding.actor != nullptr &&
            collision == &binding.primary_collision) {
            // MoveBGExecute owns the world handle for this primary
            // collision; the source call only stages actor->mBgMtx.
            return true;
        }
    }
    // Original actors may stage the collision matrix after Set() and before
    // Regist(). There is no world handle to update until registration.
    if (!collision->used_) {
        return true;
    }
    movebg::Matrix34 matrix = {};
    std::memcpy(&matrix, collision->matrix_, sizeof(matrix));
    if (!movebg_world_->update(collision->handle_, matrix)) {
        ++metrics.errors;
        return false;
    }
    ++metrics.movebg_updates;
    return true;
}

bool PspStaticModelRuntime::direct_bg_release(dBgW* collision) {
    if (!initialized_ || movebg_world_ == nullptr ||
        collision == nullptr) {
        ++metrics.errors;
        return false;
    }
    for (auto& binding : movebg_slots_) {
        if (binding.actor != nullptr &&
            collision == &binding.primary_collision) {
            if (binding.collision_active &&
                !movebg_world_->destroy(binding.handle)) {
                ++metrics.errors;
                return false;
            }
            if (binding.collision_active) {
                binding.collision_active = false;
                ++metrics.movebg_deletes;
            }
            collision->used_ = false;
            return true;
        }
    }
    if (!collision->used_) {
        ++metrics.errors;
        return false;
    }
    for (dBgW*& candidate : direct_bg_slots_) {
        if (candidate == collision) {
            if (!movebg_world_->destroy(collision->handle_)) {
                ++metrics.errors;
                return false;
            }
            candidate = nullptr;
            collision->used_ = false;
            ++metrics.movebg_deletes;
            return true;
        }
    }
    ++metrics.errors;
    return false;
}

PspActorHeapArena* PspStaticModelRuntime::acquire_arena(
    void* owner, std::uint32_t requested) {
    for (auto& arena : arenas_) {
        if (arena.owns(owner)) {
            return nullptr;
        }
    }
    for (auto& arena : arenas_) {
        if (arena.open(owner, requested)) {
            return &arena;
        }
    }
    ++metrics.actor_heap_overflows;
    return nullptr;
}

PspActorHeapArena* PspStaticModelRuntime::find_arena(void* owner) {
    for (auto& arena : arenas_) {
        if (arena.owns(owner)) {
            return &arena;
        }
    }
    return nullptr;
}

int PspStaticModelRuntime::move_bg_create(
    dBgS_MoveBgActor* actor, const char* archive,
    int collision_resource, std::uint32_t heap_size) {
    PackageSlot* package =
        find_collision_package(archive, collision_resource);
    if (package == nullptr) {
        package = find_archive_package(archive);
    }
    if (!initialized_ || actor == nullptr || package == nullptr ||
        !package->loaded ||
        (package->collision_size != 0 && movebg_world_ == nullptr) ||
        find_movebg(actor) != nullptr) {
        ++metrics.errors;
        return cPhs_ERROR_e;
    }
    std::uint32_t collision_size = 0;
    const void* collision = collision_data(
        *package, archive, collision_resource, &collision_size);
    if ((package->collision_size != 0 ||
         package->secondary_collision_size != 0) &&
        (collision == nullptr || collision_size == 0)) {
        ++metrics.errors;
        return cPhs_ERROR_e;
    }
    MoveBgSlot* binding = nullptr;
    for (auto& candidate : movebg_slots_) {
        if (candidate.actor == nullptr) {
            binding = &candidate;
            break;
        }
    }
    if (binding == nullptr) {
        ++metrics.errors;
        return cPhs_ERROR_e;
    }
    PspActorHeapArena* arena = acquire_arena(actor, heap_size);
    if (arena == nullptr || actor->CreateHeap() == 0 ||
        actor->Create() == 0) {
        if (arena != nullptr) {
            arena->release(actor);
        }
        ++metrics.errors;
        return cPhs_ERROR_e;
    }
    arena->seal();
    if (arena->peak() > metrics.actor_heap_peak) {
        metrics.actor_heap_peak = arena->peak();
    }
    metrics.actor_heap_overflows += arena->overflows();
    binding->actor = actor;
    binding->package = package;
    binding->collision_data = collision;
    binding->collision_size = collision_size;
    binding->primary_collision.collision_ = collision;
    binding->primary_collision.collision_size_ = collision_size;
    binding->primary_collision.matrix_ = &actor->mBgMtx;
    binding->primary_collision.configured_ = true;
    // MoveBG actors own their primary collision by default. Actors such as
    // daTbox_c may explicitly Release/Regist it while swapping collision.
    binding->primary_collision.used_ = true;
    actor->mpBgW = &binding->primary_collision;
    return cPhs_COMPLEATE_e;
}

int PspStaticModelRuntime::move_bg_execute(dBgS_MoveBgActor* actor) {
    MoveBgSlot* binding = find_movebg(actor);
    if (!initialized_ || actor == nullptr || binding == nullptr) {
        ++metrics.errors;
        return 0;
    }
    Mtx* matrix = nullptr;
    const int result = actor->Execute(&matrix);
    if (result == 0 || matrix == nullptr) {
        ++metrics.errors;
        return 0;
    }
    if (binding->collision_size == 0) {
        return result;
    }
    if (!binding->primary_collision.used_) {
        if (binding->collision_active) {
            if (!movebg_world_->destroy(binding->handle)) {
                ++metrics.errors;
                return 0;
            }
            binding->collision_active = false;
            ++metrics.movebg_deletes;
        }
        return result;
    }
    movebg::Matrix34 value = {};
    std::memcpy(&value, matrix, sizeof(value));
    const bool committed = binding->collision_active
        ? movebg_world_->update(binding->handle, value)
        : movebg_world_->create(
              binding->collision_data,
              binding->collision_size,
              value, &binding->handle);
    if (!committed) {
        ++metrics.errors;
        return 0;
    }
    if (binding->collision_active) {
        ++metrics.movebg_updates;
    } else {
        binding->collision_active = true;
        ++metrics.movebg_creates;
    }
    return result;
}

int PspStaticModelRuntime::move_bg_draw(dBgS_MoveBgActor* actor) {
    if (!initialized_ || actor == nullptr) {
        ++metrics.errors;
        return 0;
    }
    return actor->Draw();
}

int PspStaticModelRuntime::move_bg_delete(dBgS_MoveBgActor* actor) {
    MoveBgSlot* binding = find_movebg(actor);
    if (!initialized_ || actor == nullptr || binding == nullptr) {
        ++metrics.errors;
        return 0;
    }
    if (binding->collision_active &&
        (movebg_world_ == nullptr ||
         !movebg_world_->destroy(binding->handle))) {
        ++metrics.errors;
        return 0;
    }
    if (binding->collision_active) {
        ++metrics.movebg_deletes;
        binding->collision_active = false;
    }
    const int result = actor->Delete();
    actor->mpBgW = nullptr;
    *binding = {};
    destroy_owner(actor);
    if (PspActorHeapArena* arena = find_arena(actor)) {
        arena->release(actor);
    }
    return result;
}

bool PspStaticModelRuntime::move_bg_handle(
    const dBgS_MoveBgActor* actor, movebg::Handle* handle) const {
    const MoveBgSlot* binding = find_movebg(actor);
    if (binding == nullptr || !binding->collision_active ||
        handle == nullptr) {
        return false;
    }
    *handle = binding->handle;
    return true;
}

J3DModel* PspStaticModelRuntime::create_model(
    J3DModelData* data, void* owner) {
    PspActorHeapArena* arena = find_arena(owner);
    PackageSlot* package = find_package(data);
    if (package == nullptr) {
        ++metrics.invalid_model_data;
    }
    if (owner == nullptr) {
        ++metrics.missing_model_owner;
    }
    if (arena == nullptr) {
        ++metrics.missing_actor_arena;
    }
    if (!initialized_ || package == nullptr || owner == nullptr ||
        arena == nullptr || arena->allocate(64, 16) == nullptr) {
        ++metrics.errors;
        return nullptr;
    }
    for (auto& model : models_) {
        if (!model.active_) {
            model.data_ = data;
            model.owner_ = owner;
            model.scale_ = {1.0f, 1.0f, 1.0f};
            std::memset(
                model.mBaseTransformMtx, 0,
                sizeof(model.mBaseTransformMtx));
            model.mBaseTransformMtx[0][0] = 1.0f;
            model.mBaseTransformMtx[1][1] = 1.0f;
            model.mBaseTransformMtx[2][2] = 1.0f;
            model.active_ = true;
            ++metrics.models_created;
            metrics.models_peak = std::max(
                metrics.models_peak,
                metrics.models_created - metrics.models_destroyed);
            if (package->animation_size > 0) {
                ++metrics.animation_players_current;
                metrics.animation_players_peak = std::max(
                    metrics.animation_players_peak,
                    metrics.animation_players_current);
            }
            return &model;
        }
    }
    ++metrics.errors;
    return nullptr;
}

int PspStaticModelRuntime::entry_solid_heap(
    fopAc_ac_c* actor, heapCallbackFunc callback,
    std::uint32_t requested) {
    if (!initialized_ || actor == nullptr || callback == nullptr) {
        ++metrics.errors;
        return 0;
    }
    PspActorHeapArena* arena = acquire_arena(actor, requested);
    if (arena == nullptr || callback(actor) == 0) {
        if (arena != nullptr) {
            arena->release(actor);
        }
        ++metrics.errors;
        return 0;
    }
    arena->seal();
    if (arena->peak() > metrics.actor_heap_peak) {
        metrics.actor_heap_peak = arena->peak();
    }
    metrics.actor_heap_overflows += arena->overflows();
    return 1;
}

bool PspStaticModelRuntime::submit_model(J3DModel* model) {
    if (!initialized_ || model == nullptr || !model->active_ ||
        render_queue_ == nullptr) {
        ++metrics.errors;
        return false;
    }
    const render::Command command = {
        render::Bucket::ActorOpaque,
        kStaticModelCommand,
        0,
        model,
    };
    if (!render_queue_->enqueue(command)) {
        ++metrics.errors;
        return false;
    }
    ++metrics.render_commands;
    return true;
}

void PspStaticModelRuntime::destroy_owner(void* owner) {
    for (auto& model : models_) {
        if (model.active_ && model.owner_ == owner) {
            PackageSlot* package = find_package(model.data_);
            if (package != nullptr && package->animation_size > 0 &&
                metrics.animation_players_current > 0) {
                --metrics.animation_players_current;
            }
            model.active_ = false;
            model.owner_ = nullptr;
            model.data_ = nullptr;
            ++metrics.models_destroyed;
        }
    }
}

void* PspStaticModelRuntime::allocate_current_actor_heap(
    std::uint32_t bytes, std::uint32_t alignment) {
    void* owner = process::current_instance();
    PspActorHeapArena* arena = find_arena(owner);
    if (arena == nullptr) {
        ++metrics.missing_actor_arena;
        ++metrics.errors;
        return nullptr;
    }
    return arena->allocate(bytes, alignment);
}

bool PspStaticModelRuntime::initialized() const {
    return initialized_;
}

std::uint32_t PspStaticModelRuntime::reference_count() const {
    return references_;
}

const J3DModelData* PspStaticModelRuntime::model_data() const {
    const PackageSlot* package = find_package("L4HsMato", 4);
    return package != nullptr && package->loaded
        ? &package->model_data : nullptr;
}

void bind_model_runtime(PspStaticModelRuntime* runtime) {
    g_runtime = runtime;
}

void unbind_model_runtime() {
    g_runtime = nullptr;
}

PspStaticModelRuntime* bound_model_runtime() {
    return g_runtime;
}

}  // namespace dusk::psp::model

int dComIfG_resLoad(
    request_of_phase_process_class* phase, const char* archive) {
    return g_runtime != nullptr
        ? g_runtime->res_load(phase, archive) : cPhs_ERROR_e;
}

int dComIfG_resDelete(
    request_of_phase_process_class* phase, const char* archive) {
    return g_runtime != nullptr
        ? g_runtime->res_delete(phase, archive) : 0;
}

void* dComIfG_getObjectRes(const char* archive, int resource_id) {
    return g_runtime != nullptr
        ? g_runtime->object_resource(archive, resource_id) : nullptr;
}

J3DModel* mDoExt_J3DModel__create(
    J3DModelData* data, std::uint32_t, std::uint32_t) {
    return g_runtime != nullptr
        ? g_runtime->create_model(
              data, dusk::psp::process::current_instance())
        : nullptr;
}

void mDoExt_modelUpdateDL(J3DModel* model) {
    if (g_runtime != nullptr) {
        g_runtime->submit_model(model);
    }
}

void* operator new(
    std::size_t size, DuskPspActorHeapNewTag) noexcept {
    return g_runtime != nullptr
        ? g_runtime->allocate_current_actor_heap(
              static_cast<std::uint32_t>(size), alignof(std::max_align_t))
        : nullptr;
}

int dBgW::Set(cBgD_t* collision, u32, Mtx* matrix) {
    return g_runtime != nullptr &&
                   g_runtime->direct_bg_set(this, collision, matrix)
        ? 0 : 1;
}

void dBgW::Move() {
    if (g_runtime != nullptr && used_) {
        g_runtime->direct_bg_move(this);
    }
}

int dBgS_CompatWorld::Regist(dBgW* collision, fopAc_ac_c*) {
    return g_runtime != nullptr &&
                   g_runtime->direct_bg_register(collision)
        ? 0 : 1;
}

int dBgS_CompatWorld::Release(dBgW* collision) {
    return g_runtime != nullptr &&
                   g_runtime->direct_bg_release(collision)
        ? 0 : 1;
}

dBgS_CompatWorld& dComIfG_Bgsp() {
    static dBgS_CompatWorld world;
    return world;
}

int dBgS_MoveBgActor::MoveBGCreate(
    const char* archive, int collision_resource,
    dBgS_MoveBGProc, std::uint32_t heap_size, MtxP) {
    return g_runtime != nullptr
        ? g_runtime->move_bg_create(
              this, archive, collision_resource, heap_size)
        : cPhs_ERROR_e;
}

int dBgS_MoveBgActor::MoveBGExecute() {
    return g_runtime != nullptr
        ? g_runtime->move_bg_execute(this) : 0;
}

int dBgS_MoveBgActor::MoveBGDraw() {
    return g_runtime != nullptr
        ? g_runtime->move_bg_draw(this) : 0;
}

int dBgS_MoveBgActor::MoveBGDelete() {
    return g_runtime != nullptr
        ? g_runtime->move_bg_delete(this) : 0;
}

void dBgS_MoveBGProc_TypicalRotY() {}

int fopAcM_entrySolidHeap(
    fopAc_ac_c* actor, heapCallbackFunc callback, u32 size) {
    return g_runtime != nullptr
        ? g_runtime->entry_solid_heap(actor, callback, size) : 0;
}

void fopAcM_seStartLevel(fopAc_ac_c*, int, s16) {}

const void* J3DModelData::model_bytes() const {
    return model_bytes_;
}

std::uint32_t J3DModelData::model_size() const {
    return model_size_;
}

const void* J3DModelData::texture_bytes() const {
    return texture_bytes_;
}

std::uint32_t J3DModelData::texture_size() const {
    return texture_size_;
}

void J3DModel::setBaseScale(const cXyz& scale) {
    scale_ = scale;
}

void J3DModel::setBaseTRMtx(MtxP matrix) {
    if (matrix != nullptr) {
        std::memcpy(
            mBaseTransformMtx, matrix, sizeof(mBaseTransformMtx));
        if (g_runtime != nullptr) {
            ++g_runtime->metrics.matrices_from_original_logic;
        }
    }
}

Mtx& J3DModel::getBaseTRMtx() {
    return mBaseTransformMtx;
}

const Mtx& J3DModel::getBaseTRMtx() const {
    return mBaseTransformMtx;
}

J3DModelData* J3DModel::getModelData() {
    return data_;
}

const J3DModelData* J3DModel::getModelData() const {
    return data_;
}

const cXyz& J3DModel::getBaseScale() const {
    return scale_;
}

void* J3DModel::owner() const {
    return owner_;
}

bool J3DModel::active() const {
    return active_;
}

dScnKy_env_light_c g_env_light;

void dScnKy_env_light_c::settingTevStruct(
    int, const cXyz*, dKy_tevstr_c* tev) {
    if (tev != nullptr) {
        tev->color = 0xFFFFFFFFu;
    }
}

void dScnKy_env_light_c::setLightTevColorType_MAJI(
    J3DModel*, dKy_tevstr_c*) {}

void dusk_psp_compat_set_cull_box(
    fopAc_ac_c* actor, const J3DModelData* model_data) {
    if (actor == nullptr || model_data == nullptr ||
        model_data->model_bytes() == nullptr ||
        model_data->model_size() < 72) {
        return;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(
        model_data->model_bytes());
    actor->cull_min.set(
        dusk::psp::room::read_f32(bytes + 48),
        dusk::psp::room::read_f32(bytes + 52),
        dusk::psp::room::read_f32(bytes + 56));
    actor->cull_max.set(
        dusk::psp::room::read_f32(bytes + 60),
        dusk::psp::room::read_f32(bytes + 64),
        dusk::psp::room::read_f32(bytes + 68));
}
