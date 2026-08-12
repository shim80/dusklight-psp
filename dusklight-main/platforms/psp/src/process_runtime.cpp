#include "dusk/psp/process_runtime.hpp"

#include <cstring>

namespace dusk::psp::process {
namespace {

const process_profile_definition& base_profile(
    const actor_process_profile_definition& profile) {
    return profile.base.base;
}

const actor_method_class* actor_methods(
    const actor_process_profile_definition& profile) {
    return profile.sub_method;
}

void* g_current_instance = nullptr;
PspProcessManager* g_manager = nullptr;

class CurrentInstanceScope {
public:
    explicit CurrentInstanceScope(void* instance) {
        previous_ = g_current_instance;
        g_current_instance = instance;
    }
    ~CurrentInstanceScope() {
        g_current_instance = previous_;
    }

private:
    void* previous_;
};

}  // namespace

void PspProcessManager::initialize() {
    std::memset(profiles_, 0, sizeof(profiles_));
    std::memset(slots_, 0, sizeof(slots_));
    std::memset(externals_, 0, sizeof(externals_));
    std::memset(lifecycle_events_, 0, sizeof(lifecycle_events_));
    profile_count_ = 0;
    active_count_ = 0;
    lifecycle_event_count_ = 0;
    lifecycle_events_dropped_ = 0;
    next_lifecycle_sequence_ = 1;
    next_process_uid_ = 1;
    metrics = {};
    initialized_ = true;
}

bool PspProcessManager::register_profile(
    const actor_process_profile_definition* profile) {
    if (!initialized_ || profile == nullptr ||
        base_profile(*profile).process_size > kInstanceStorage ||
        actor_methods(*profile) == nullptr ||
        profile_count_ >= kProfileCapacity) {
        ++metrics.errors;
        return false;
    }
    const std::int16_t process_id = base_profile(*profile).name;
    if (find_profile(process_id) != nullptr) {
        ++metrics.duplicate_profiles;
        ++metrics.errors;
        return false;
    }
    profiles_[profile_count_++] = {
        profile, process_id,
        static_cast<std::uint16_t>(profile->base.priority), {}};
    profiles_[profile_count_ - 1].metrics.profile_register_calls = 1;
    ++metrics.profile_register_calls;
    return true;
}

bool PspProcessManager::register_profile(
    const actor_process_profile_definition2* profile) {
    return profile != nullptr && register_profile(&profile->base);
}

const PspProcessManager::RegisteredProfile*
PspProcessManager::find_profile(std::int16_t process_id) const {
    for (std::uint16_t index = 0; index < profile_count_; ++index) {
        if (profiles_[index].process_id == process_id) {
            return &profiles_[index];
        }
    }
    return nullptr;
}

bool PspProcessManager::create(
    const CreateInput& input, ProcessHandle* handle) {
    const RegisteredProfile* registered =
        find_profile(input.process_id);
    if (!initialized_ || registered == nullptr || handle == nullptr) {
        ++metrics.errors;
        return false;
    }
    std::uint16_t index = 0;
    while (index < kInstanceCapacity && slots_[index].active) {
        ++index;
    }
    if (index == kInstanceCapacity) {
        ++metrics.errors;
        return false;
    }
    Slot& slot = slots_[index];
    const std::uint16_t next_generation =
        slot.generation == 0xFFFFu
            ? 1u : static_cast<std::uint16_t>(slot.generation + 1u);
    std::memset(slot.storage, 0, sizeof(slot.storage));
    fopAc_ac_c* actor =
        reinterpret_cast<fopAc_ac_c*>(slot.storage);
    actor->parameters = input.parameters;
    actor->process_id = input.process_id;
    actor->home.roomNo = input.room;
    actor->current.roomNo = input.room;
    actor->home.pos.set(
        input.position[0], input.position[1], input.position[2]);
    actor->current.pos = actor->home.pos;
    actor->home.angle.set(
        input.rotation[0], input.rotation[1], input.rotation[2]);
    actor->current.angle = actor->home.angle;
    actor->shape_angle = actor->home.angle;
    actor->scale.set(input.scale[0], input.scale[1], input.scale[2]);
    slot.profile = registered->definition;
    slot.profile_index = static_cast<std::uint16_t>(
        registered - profiles_);
    slot.process_uid = next_process_uid_++;
    if (next_process_uid_ == fpcM_ERROR_PROCESS_ID_e) {
        next_process_uid_ = 1;
    }
    Metrics& profile_metrics = profiles_[slot.profile_index].metrics;
    slot.generation = next_generation;
    slot.room = input.room;
    slot.layer = input.layer;
    slot.source_table_hash = input.source_table_hash;
    slot.source_index = input.source_index;
    slot.source_name_hash = input.source_name_hash;
    slot.active = true;
    ++active_count_;
    const process_method_func create_method =
        actor_methods(*slot.profile)->base.base.create_method;
    int phase = cPhs_INIT_e;
    for (std::uint8_t attempt = 0; attempt < 8 &&
         phase != cPhs_COMPLEATE_e; ++attempt) {
        ++metrics.create_calls;
        ++profile_metrics.create_calls;
        if (create_method == nullptr) {
            phase = cPhs_ERROR_e;
            break;
        }
        record_lifecycle(
            slot, ProcessLifecyclePhase::CreateEnter, phase);
        CurrentInstanceScope scope(slot.storage);
        phase = create_method(slot.storage);
        record_lifecycle(
            slot, ProcessLifecyclePhase::CreateExit, phase);
        if (phase != cPhs_INIT_e && phase != cPhs_LOADING_e &&
            phase != cPhs_NEXT_e && phase != cPhs_COMPLEATE_e) {
            phase = cPhs_ERROR_e;
        }
    }
    if (phase != cPhs_COMPLEATE_e) {
        slot.active = false;
        slot.profile = nullptr;
        --active_count_;
        ++metrics.errors;
        ++profile_metrics.errors;
        return false;
    }
    if (active_count_ > metrics.peak_instances) {
        metrics.peak_instances = active_count_;
    }
    *handle = {index, slot.generation};
    return true;
}

bool PspProcessManager::execute_all() {
    if (!initialized_) {
        ++metrics.errors;
        return false;
    }
    for (std::uint16_t index = 0; index < kInstanceCapacity; ++index) {
        Slot& slot = slots_[index];
        if (!slot.active) {
            continue;
        }
        const process_method_func execute_method =
            actor_methods(*slot.profile)->base.base.execute_method;
        ++metrics.execute_calls;
        ++profiles_[slot.profile_index].metrics.execute_calls;
        if (!slot.first_execute_observed) {
            record_lifecycle(
                slot, ProcessLifecyclePhase::FirstExecuteEnter, 0);
        }
        CurrentInstanceScope scope(slot.storage);
        const int execute_result = execute_method != nullptr
            ? execute_method(slot.storage) : 0;
        if (!slot.first_execute_observed) {
            record_lifecycle(
                slot, ProcessLifecyclePhase::FirstExecuteExit,
                execute_result);
            slot.first_execute_observed = true;
        }
        if (execute_result == 0) {
            ++metrics.errors;
            ++profiles_[slot.profile_index].metrics.errors;
            return false;
        }
        if (slot.delete_requested && !destroy_slot(index)) {
            return false;
        }
    }
    return true;
}

bool PspProcessManager::draw_all() {
    if (!initialized_) {
        ++metrics.errors;
        return false;
    }
    for (std::uint16_t index = 0; index < kInstanceCapacity; ++index) {
        Slot& slot = slots_[index];
        if (!slot.active) {
            continue;
        }
        ++metrics.draw_calls;
        ++profiles_[slot.profile_index].metrics.draw_calls;
        const process_method_func draw_method =
            actor_methods(*slot.profile)->base.draw_method;
        CurrentInstanceScope scope(slot.storage);
        if (draw_method != nullptr && draw_method(slot.storage) == 0) {
            ++metrics.errors;
            ++profiles_[slot.profile_index].metrics.errors;
            return false;
        }
    }
    return true;
}

bool PspProcessManager::destroy(ProcessHandle handle) {
    if (!handle_valid(handle)) {
        ++metrics.stale_handles;
        return false;
    }
    return destroy_slot(handle.slot);
}

bool PspProcessManager::request_delete(void* instance) {
    if (!initialized_ || instance == nullptr) {
        ++metrics.errors;
        return false;
    }
    for (auto& slot : slots_) {
        if (slot.active && slot.storage == instance) {
            slot.delete_requested = true;
            return true;
        }
    }
    ++metrics.calls_after_delete;
    return false;
}

bool PspProcessManager::destroy_slot(std::uint16_t index) {
    Slot& slot = slots_[index];
    const actor_method_class* methods = actor_methods(*slot.profile);
    ++metrics.is_delete_calls;
    ++profiles_[slot.profile_index].metrics.is_delete_calls;
    if (methods->base.base.is_delete_method != nullptr &&
        [&]() {
            CurrentInstanceScope scope(slot.storage);
            return methods->base.base.is_delete_method(slot.storage);
        }() == 0) {
        return false;
    }
    ++metrics.delete_calls;
    ++profiles_[slot.profile_index].metrics.delete_calls;
    if (methods->base.base.delete_method != nullptr &&
        [&]() {
            CurrentInstanceScope scope(slot.storage);
            return methods->base.base.delete_method(slot.storage);
        }() == 0) {
        ++metrics.errors;
        ++profiles_[slot.profile_index].metrics.errors;
        return false;
    }
    slot.active = false;
    slot.profile = nullptr;
    slot.delete_requested = false;
    --active_count_;
    return true;
}

void PspProcessManager::destroy_room(std::int8_t room) {
    for (std::uint16_t index = 0; index < kInstanceCapacity; ++index) {
        if (slots_[index].active && slots_[index].room == room &&
            !destroy_slot(index)) {
            ++metrics.errors;
        }
    }
    for (auto& external : externals_) {
        if (external.active && external.room == room) {
            external = {};
        }
    }
}

bool PspProcessManager::add_external(
    void* instance, std::int16_t process_id, std::int8_t room,
    fpc_ProcID* id) {
    if (!initialized_ || instance == nullptr || id == nullptr ||
        id_of(instance) != fpcM_ERROR_PROCESS_ID_e) {
        ++metrics.errors;
        return false;
    }
    for (auto& external : externals_) {
        if (!external.active) {
            external.instance = instance;
            external.process_uid = next_process_uid_++;
            if (next_process_uid_ == fpcM_ERROR_PROCESS_ID_e) {
                next_process_uid_ = 1;
            }
            external.process_id = process_id;
            external.room = room;
            external.active = true;
            *id = external.process_uid;
            return true;
        }
    }
    ++metrics.errors;
    return false;
}

bool PspProcessManager::remove_external(void* instance) {
    for (auto& external : externals_) {
        if (external.active && external.instance == instance) {
            external = {};
            return true;
        }
    }
    ++metrics.errors;
    return false;
}

void* PspProcessManager::search(fpcLyIt_JudgeFunc judge, void* data) {
    if (!initialized_ || judge == nullptr) {
        ++metrics.errors;
        return nullptr;
    }
    for (auto& slot : slots_) {
        if (slot.active) {
            if (void* result = judge(slot.storage, data)) {
                return result;
            }
        }
    }
    for (auto& external : externals_) {
        if (external.active) {
            if (void* result = judge(external.instance, data)) {
                return result;
            }
        }
    }
    return nullptr;
}

void* PspProcessManager::search_by_id(fpc_ProcID id) {
    for (auto& slot : slots_) {
        if (slot.active && slot.process_uid == id) {
            return slot.storage;
        }
    }
    for (auto& external : externals_) {
        if (external.active && external.process_uid == id) {
            return external.instance;
        }
    }
    return nullptr;
}

void* PspProcessManager::first_instance(std::int16_t process_id) {
    for (auto& slot : slots_) {
        if (slot.active &&
            base_profile(*slot.profile).name == process_id) {
            return slot.storage;
        }
    }
    return nullptr;
}

const void* PspProcessManager::first_instance(
    std::int16_t process_id) const {
    return const_cast<PspProcessManager*>(this)->first_instance(
        process_id);
}

fpc_ProcID PspProcessManager::id_of(const void* instance) const {
    for (const auto& slot : slots_) {
        if (slot.active && slot.storage == instance) {
            return slot.process_uid;
        }
    }
    for (const auto& external : externals_) {
        if (external.active && external.instance == instance) {
            return external.process_uid;
        }
    }
    return fpcM_ERROR_PROCESS_ID_e;
}

std::int16_t PspProcessManager::process_id_of(const void* instance) const {
    for (const auto& slot : slots_) {
        if (slot.active && slot.storage == instance) {
            return base_profile(*slot.profile).name;
        }
    }
    for (const auto& external : externals_) {
        if (external.active && external.instance == instance) {
            return external.process_id;
        }
    }
    return -1;
}

void PspProcessManager::shutdown() {
    for (std::uint16_t index = 0; index < kInstanceCapacity; ++index) {
        if (slots_[index].active && !destroy_slot(index)) {
            ++metrics.errors;
        }
    }
    std::memset(externals_, 0, sizeof(externals_));
    initialized_ = false;
}

bool PspProcessManager::handle_valid(ProcessHandle handle) const {
    return initialized_ && handle.slot < kInstanceCapacity &&
           slots_[handle.slot].active &&
           slots_[handle.slot].generation == handle.generation;
}

void* PspProcessManager::instance(ProcessHandle handle) {
    return handle_valid(handle) ? slots_[handle.slot].storage : nullptr;
}

const void* PspProcessManager::instance(ProcessHandle handle) const {
    return handle_valid(handle) ? slots_[handle.slot].storage : nullptr;
}

std::uint16_t PspProcessManager::active_count() const {
    return active_count_;
}

std::uint16_t PspProcessManager::profile_count() const {
    return profile_count_;
}

bool PspProcessManager::profile_registered(std::int16_t process_id) const {
    return find_profile(process_id) != nullptr;
}

const Metrics* PspProcessManager::profile_metrics(
    std::int16_t process_id) const {
    const RegisteredProfile* profile = find_profile(process_id);
    return profile != nullptr ? &profile->metrics : nullptr;
}

bool PspProcessManager::observe_active(
    std::uint16_t ordinal, ProcessObservation* observation) const {
    if (!initialized_ || observation == nullptr) {
        return false;
    }
    for (const auto& slot : slots_) {
        if (!slot.active) {
            continue;
        }
        if (ordinal != 0) {
            --ordinal;
            continue;
        }
        *observation = {
            reinterpret_cast<const fopAc_ac_c*>(slot.storage),
            slot.process_uid,
            base_profile(*slot.profile).name,
            slot.room,
            slot.layer,
            slot.source_table_hash,
            slot.source_index,
            slot.source_name_hash,
            slot.generation,
            profiles_[slot.profile_index].metrics,
        };
        return true;
    }
    return false;
}

std::uint16_t PspProcessManager::lifecycle_event_count() const {
    return lifecycle_event_count_;
}

bool PspProcessManager::lifecycle_event(
    std::uint16_t index, ProcessLifecycleEvent* event) const {
    if (event == nullptr || index >= lifecycle_event_count_) {
        return false;
    }
    *event = lifecycle_events_[index];
    return true;
}

std::uint32_t PspProcessManager::lifecycle_events_dropped() const {
    return lifecycle_events_dropped_;
}

void PspProcessManager::record_lifecycle(
    const Slot& slot, ProcessLifecyclePhase phase,
    std::int32_t callback_result) {
    if (lifecycle_event_count_ >= kLifecycleEventCapacity) {
        ++lifecycle_events_dropped_;
        return;
    }
    lifecycle_events_[lifecycle_event_count_++] = {
        next_lifecycle_sequence_++,
        phase,
        slot.process_uid,
        base_profile(*slot.profile).name,
        slot.room,
        slot.layer,
        slot.source_table_hash,
        slot.source_index,
        slot.source_name_hash,
        slot.generation,
        callback_result,
    };
}

void* current_instance() {
    return g_current_instance;
}

void bind_process_manager(PspProcessManager* manager) {
    g_manager = manager;
}

void unbind_process_manager() {
    g_manager = nullptr;
}

}  // namespace dusk::psp::process

int fopAcM_delete(fopAc_ac_c* actor) {
    return dusk::psp::process::g_manager != nullptr &&
           dusk::psp::process::g_manager->request_delete(actor);
}

void* fpcM_Search(fpcLyIt_JudgeFunc judge, void* data) {
    return dusk::psp::process::g_manager != nullptr
        ? dusk::psp::process::g_manager->search(judge, data) : nullptr;
}

void* fopAcM_SearchByID(fpc_ProcID id) {
    return dusk::psp::process::g_manager != nullptr
        ? dusk::psp::process::g_manager->search_by_id(id) : nullptr;
}

fpc_ProcID fopAcM_GetID(const void* actor) {
    return dusk::psp::process::g_manager != nullptr
        ? dusk::psp::process::g_manager->id_of(actor)
        : fpcM_ERROR_PROCESS_ID_e;
}

s16 fpcM_GetProfName(const void* actor) {
    return dusk::psp::process::g_manager != nullptr
        ? dusk::psp::process::g_manager->process_id_of(actor) : -1;
}

BOOL fopAc_IsActor(const void* actor) {
    return fpcM_GetProfName(actor) >= 0 ? TRUE : FALSE;
}
