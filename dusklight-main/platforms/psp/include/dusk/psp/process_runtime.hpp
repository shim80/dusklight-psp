#ifndef DUSK_PSP_PROCESS_RUNTIME_HPP
#define DUSK_PSP_PROCESS_RUNTIME_HPP

#include "f_op/f_op_actor_mng.h"

#include <cstdint>

namespace dusk::psp::process {

constexpr std::uint16_t kProfileCapacity = 64;
constexpr std::uint16_t kInstanceCapacity = 128;
// The native PSP declaration of the real daTbox_c is 576 bytes on the host
// ABI. Keep a small, fixed margin for the 32-bit PSPSDK ABI and bounded
// compatibility metadata without introducing per-process allocation.
constexpr std::uint32_t kInstanceStorage = 640;
constexpr std::uint16_t kExternalCapacity = 32;
constexpr std::uint16_t kLifecycleEventCapacity = 512;

struct ProcessHandle {
    std::uint16_t slot;
    std::uint16_t generation;
};

struct CreateInput {
    std::int16_t process_id;
    std::uint32_t parameters;
    float position[3];
    std::int16_t rotation[3];
    float scale[3];
    std::int8_t room;
    std::uint32_t source_table_hash;
    std::uint16_t source_index;
    std::uint32_t source_name_hash;
    std::int8_t layer;
};

struct Metrics {
    std::uint32_t profile_register_calls;
    std::uint32_t create_calls;
    std::uint32_t execute_calls;
    std::uint32_t draw_calls;
    std::uint32_t is_delete_calls;
    std::uint32_t delete_calls;
    std::uint32_t errors;
    std::uint32_t duplicate_profiles;
    std::uint32_t stale_handles;
    std::uint32_t calls_after_delete;
    std::uint32_t peak_instances;
};

struct ProcessObservation {
    const fopAc_ac_c* actor;
    fpc_ProcID process_uid;
    std::int16_t process_id;
    std::int8_t room;
    std::int8_t layer;
    std::uint32_t source_table_hash;
    std::uint16_t source_index;
    std::uint32_t source_name_hash;
    std::uint16_t generation;
    Metrics metrics;
};

enum class ProcessLifecyclePhase : std::uint8_t {
    CreateEnter,
    CreateExit,
    FirstExecuteEnter,
    FirstExecuteExit,
};

struct ProcessLifecycleEvent {
    std::uint32_t sequence;
    ProcessLifecyclePhase phase;
    fpc_ProcID process_uid;
    std::int16_t process_id;
    std::int8_t room;
    std::int8_t layer;
    std::uint32_t source_table_hash;
    std::uint16_t source_index;
    std::uint32_t source_name_hash;
    std::uint16_t generation;
    std::int32_t callback_result;
};

class PspProcessManager {
public:
    void initialize();
    bool register_profile(
        const actor_process_profile_definition* profile);
    bool register_profile(
        const actor_process_profile_definition2* profile);
    bool create(const CreateInput& input, ProcessHandle* handle);
    bool execute_all();
    bool draw_all();
    bool destroy(ProcessHandle handle);
    bool request_delete(void* instance);
    void destroy_room(std::int8_t room);
    bool add_external(
        void* instance, std::int16_t process_id, std::int8_t room,
        fpc_ProcID* id);
    bool remove_external(void* instance);
    void* search(fpcLyIt_JudgeFunc judge, void* data);
    void* search_by_id(fpc_ProcID id);
    void* first_instance(std::int16_t process_id);
    const void* first_instance(std::int16_t process_id) const;
    fpc_ProcID id_of(const void* instance) const;
    std::int16_t process_id_of(const void* instance) const;
    void shutdown();

    bool handle_valid(ProcessHandle handle) const;
    void* instance(ProcessHandle handle);
    const void* instance(ProcessHandle handle) const;
    std::uint16_t active_count() const;
    std::uint16_t profile_count() const;
    bool profile_registered(std::int16_t process_id) const;
    const Metrics* profile_metrics(std::int16_t process_id) const;
    bool observe_active(
        std::uint16_t ordinal, ProcessObservation* observation) const;
    std::uint16_t lifecycle_event_count() const;
    bool lifecycle_event(
        std::uint16_t index, ProcessLifecycleEvent* event) const;
    std::uint32_t lifecycle_events_dropped() const;

    Metrics metrics = {};

private:
    struct RegisteredProfile {
        const actor_process_profile_definition* definition;
        std::int16_t process_id;
        std::uint16_t priority;
        Metrics metrics;
    };

    struct Slot {
        alignas(16) std::uint8_t storage[kInstanceStorage];
        const actor_process_profile_definition* profile;
        std::uint16_t generation;
        std::uint16_t profile_index;
        fpc_ProcID process_uid;
        std::int8_t room;
        std::int8_t layer;
        std::uint32_t source_table_hash;
        std::uint16_t source_index;
        std::uint32_t source_name_hash;
        bool active;
        bool delete_requested;
        bool first_execute_observed;
    };

    struct External {
        void* instance;
        fpc_ProcID process_uid;
        std::int16_t process_id;
        std::int8_t room;
        bool active;
    };

    const RegisteredProfile* find_profile(std::int16_t process_id) const;
    bool destroy_slot(std::uint16_t slot);
    void record_lifecycle(
        const Slot& slot, ProcessLifecyclePhase phase,
        std::int32_t callback_result);

    RegisteredProfile profiles_[kProfileCapacity] = {};
    Slot slots_[kInstanceCapacity] = {};
    External externals_[kExternalCapacity] = {};
    ProcessLifecycleEvent lifecycle_events_[kLifecycleEventCapacity] = {};
    std::uint16_t profile_count_ = 0;
    std::uint16_t active_count_ = 0;
    std::uint16_t lifecycle_event_count_ = 0;
    std::uint32_t lifecycle_events_dropped_ = 0;
    std::uint32_t next_lifecycle_sequence_ = 1;
    fpc_ProcID next_process_uid_ = 1;
    bool initialized_ = false;
};

void* current_instance();
void bind_process_manager(PspProcessManager* manager);
void unbind_process_manager();

}  // namespace dusk::psp::process

#endif
