#ifndef DUSK_PSP_PARITY_TRACE_HPP
#define DUSK_PSP_PARITY_TRACE_HPP

#include "dusk/psp/parity_identity.hpp"

#include <cstddef>
#include <cstdint>

namespace dusk::psp::parity {

constexpr std::uint32_t kDtrcSchemaVersion = 3;
constexpr std::uint32_t kDtrcSchemaRevision = 1;
constexpr std::size_t kTraceLineCapacity = 4096;

enum class LifecycleCheckpoint : std::uint8_t {
    ActorConstructed,
    ActorCreateEnter,
    ActorCreateExit,
    ActorFirstExecuteEnter,
    ActorFirstExecuteExit,
    AnimationUpdateEnter,
    AnimationUpdateExit,
    GroundingEnter,
    GroundingExit,
    CameraUpdate,
    DrawPrepare,
    DrawSubmit,
    FramePresent,
};

const char* lifecycle_checkpoint_name(LifecycleCheckpoint checkpoint);

enum class EventType : std::uint8_t {
    ActorTransform,
    ActorOrigin,
    ActorState,
    ActorCollision,
    ActorInteraction,
    ActorCulling,
    ActorShadow,
    ModelInstance,
    ModelBaseMatrix,
    ModelLocalOrigin,
    ModelBounds,
    ModelDrawMatrix,
    JointLocal,
    JointGlobal,
    JointReference,
    AnimationClip,
    AnimationFrame,
    AnimationRoot,
    CollisionMatrix,
    CollisionBounds,
    MoveBgMatrix,
    CameraState,
    UiPaneTransform,
    ResourceLifecycle,
    RenderSubmission,
    SceneCheckpoint,
    BehaviorCheckpoint,
    InputChange,
    LocomotionStateChange,
    AnimationChange,
    TurnStart,
    TurnEnd,
    Stop,
    FloorContact,
    LifecycleCheckpoint,
};

const char* event_type_name(EventType type);

struct EventHeader {
    const char* build_identity;
    const char* run_id;
    const char* scenario_id;
    std::uint32_t frame;
    std::uint32_t game_tick;
    ParitySceneId scene_id;
    const char* stage;
    std::int16_t room;
    std::int16_t layer;
    std::uint32_t source_table;
    std::uint16_t source_index;
    std::uint32_t source_name_hash;
    std::uint16_t process_id;
    std::uint16_t profile_id;
    std::uint32_t actor_generation;
    std::uint32_t model_resource_id;
    std::uint16_t model_instance_id;
    LifecycleCheckpoint lifecycle_checkpoint;
};

using TraceSink = bool (*)(
    void* user, const char* line, std::size_t line_size);

class DtrcV3Writer {
public:
    bool initialize(TraceSink sink, void* user);
    bool emit(
        const EventHeader& header, EventType type,
        const char* payload_json);
    bool healthy() const;
    std::uint32_t events_written() const;
    std::uint32_t dropped_events() const;

private:
    TraceSink sink_ = nullptr;
    void* user_ = nullptr;
    std::uint32_t events_written_ = 0;
    std::uint32_t dropped_events_ = 0;
    bool initialized_ = false;
};

}  // namespace dusk::psp::parity

#endif
