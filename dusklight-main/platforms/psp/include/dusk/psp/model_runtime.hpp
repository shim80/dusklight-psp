#ifndef DUSK_PSP_MODEL_RUNTIME_HPP
#define DUSK_PSP_MODEL_RUNTIME_HPP

#include "dusk/psp/actor_heap_arena.hpp"
#include "dusk/psp/movebg_runtime.hpp"
#include "dusk/psp/render_queue.hpp"
#include "dusk/psp/resource_manager.hpp"
#include "d/d_bg_s_movebg_actor.h"
#include "d/d_bg_w.h"
#include "m_Do/m_Do_ext.h"

#include <cstdint>

namespace dusk::psp::model {

constexpr std::uint16_t kStaticModelCommand = 0x100;

struct Metrics {
    std::uint32_t archive_requests;
    std::uint32_t resource_get_calls;
    std::uint32_t resource_release_calls;
    std::uint32_t models_created;
    std::uint32_t models_destroyed;
    std::uint32_t models_peak;
    std::uint32_t animation_players_current;
    std::uint32_t animation_players_peak;
    std::uint32_t matrices_from_original_logic;
    std::uint32_t render_commands;
    std::uint32_t errors;
    std::uint32_t actor_heap_peak;
    std::uint32_t actor_heap_overflows;
    std::uint32_t invalid_model_data;
    std::uint32_t missing_model_owner;
    std::uint32_t missing_actor_arena;
    std::uint32_t runtime_material_count;
    std::uint32_t runtime_texture_count;
    std::uint32_t runtime_texture_bytes;
    std::uint32_t movebg_creates;
    std::uint32_t movebg_updates;
    std::uint32_t movebg_deletes;
    std::uint32_t movebg_matrix_mismatches;
};

class PspStaticModelRuntime {
public:
    static constexpr std::uint16_t kModelCapacity = 16;
    static constexpr std::uint16_t kPackageCapacity = 7;
    static constexpr std::uint16_t kMoveBgCapacity = 16;
    static constexpr std::uint32_t kModelBytesCapacity = 131072;
    static constexpr std::uint32_t kTextureBytesCapacity = 262144;
    static constexpr std::uint32_t kAnimationBytesCapacity = 32768;
    static constexpr std::uint32_t kCollisionBytesCapacity = 16384;

    bool initialize(
        resources::PspResourceManager* resources,
        render::PspRenderQueue* render_queue,
        movebg::PspMoveBgWorld* movebg_world = nullptr);
    void shutdown();

    int res_load(
        request_of_phase_process_class* phase, const char* archive);
    int res_delete(
        request_of_phase_process_class* phase, const char* archive);
    void* object_resource(const char* archive, int resource_id);

    int move_bg_create(
        dBgS_MoveBgActor* actor, const char* archive,
        int collision_resource, std::uint32_t heap_size);
    int move_bg_execute(dBgS_MoveBgActor* actor);
    int move_bg_draw(dBgS_MoveBgActor* actor);
    int move_bg_delete(dBgS_MoveBgActor* actor);
    bool move_bg_handle(
        const dBgS_MoveBgActor* actor, movebg::Handle* handle) const;
    bool direct_bg_set(dBgW* collision, const void* data, Mtx* matrix);
    bool direct_bg_register(dBgW* collision);
    bool direct_bg_move(dBgW* collision);
    bool direct_bg_release(dBgW* collision);

    J3DModel* create_model(J3DModelData* data, void* owner);
    int entry_solid_heap(
        fopAc_ac_c* actor, heapCallbackFunc callback,
        std::uint32_t requested);
    bool submit_model(J3DModel* model);
    void destroy_owner(void* owner);
    void* allocate_current_actor_heap(
        std::uint32_t bytes, std::uint32_t alignment);

    bool initialized() const;
    std::uint32_t reference_count() const;
    const J3DModelData* model_data() const;
    Metrics metrics = {};

private:
    struct PackageSlot {
        alignas(16) std::uint8_t model_bytes[kModelBytesCapacity];
        alignas(16) std::uint8_t texture_bytes[kTextureBytesCapacity];
        alignas(16) std::uint8_t animation_bytes[kAnimationBytesCapacity];
        alignas(16) std::uint8_t collision_bytes[kCollisionBytesCapacity];
        alignas(16) std::uint8_t
            secondary_collision_bytes[kCollisionBytesCapacity];
        resources::PspResourceHandle model_handle;
        resources::PspResourceHandle texture_handle;
        resources::PspResourceHandle animation_handle;
        resources::PspResourceHandle collision_handle;
        resources::PspResourceHandle secondary_collision_handle;
        std::uint32_t model_size;
        std::uint32_t texture_size;
        std::uint32_t animation_size;
        std::uint32_t collision_size;
        std::uint32_t secondary_collision_size;
        std::uint32_t references;
        J3DModelData model_data;
        J3DAnmTransform animation;
        bool loaded;
    };

    struct MoveBgSlot {
        dBgS_MoveBgActor* actor;
        PackageSlot* package;
        const void* collision_data;
        std::uint32_t collision_size;
        dBgW primary_collision;
        movebg::Handle handle;
        bool collision_active;
    };

    bool load_archive(const char* archive);
    void release_archive(const char* archive);
    void clear_package(PackageSlot& package);
    PackageSlot* find_package(const char* archive, int resource_id);
    const PackageSlot* find_package(
        const char* archive, int resource_id) const;
    PackageSlot* find_package(J3DModelData* data);
    PackageSlot* find_archive_package(const char* archive);
    PackageSlot* find_collision_package(
        const char* archive, int resource_id);
    const void* collision_data(
        const PackageSlot& package, const char* archive,
        int resource_id, std::uint32_t* size) const;
    MoveBgSlot* find_movebg(dBgS_MoveBgActor* actor);
    const MoveBgSlot* find_movebg(
        const dBgS_MoveBgActor* actor) const;
    PspActorHeapArena* acquire_arena(
        void* owner, std::uint32_t requested);
    PspActorHeapArena* find_arena(void* owner);

    resources::PspResourceManager* resources_ = nullptr;
    render::PspRenderQueue* render_queue_ = nullptr;
    movebg::PspMoveBgWorld* movebg_world_ = nullptr;
    PackageSlot packages_[kPackageCapacity] = {};
    MoveBgSlot movebg_slots_[kMoveBgCapacity] = {};
    dBgW* direct_bg_slots_[kMoveBgCapacity] = {};
    J3DModel models_[kModelCapacity] = {};
    PspActorHeapArena arenas_[kModelCapacity] = {};
    std::uint32_t references_ = 0;
    bool initialized_ = false;
};

void bind_model_runtime(PspStaticModelRuntime* runtime);
void unbind_model_runtime();
PspStaticModelRuntime* bound_model_runtime();

}  // namespace dusk::psp::model

#endif
