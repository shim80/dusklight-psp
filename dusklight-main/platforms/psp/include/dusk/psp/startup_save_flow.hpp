#ifndef DUSK_PSP_STARTUP_SAVE_FLOW_HPP
#define DUSK_PSP_STARTUP_SAVE_FLOW_HPP

#include "dusk/psp/save_storage.hpp"
#include "dusk/psp/startup_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dusk::psp::startup {

enum class SaveFlowPhase : std::uint8_t {
    Startup = 0,
    FileSelect,
    Transition,
    HandoffReady,
    Gameplay,
    Error,
};

struct SaveFlowInput {
    bool up = false;
    bool down = false;
    bool confirm = false;
    bool start = false;
    bool resources_ready = false;
    bool source_event_complete = false;
};

class StartupSaveFlow {
public:
    bool initialize(
        const PackageView& package,
        std::uint32_t available_capabilities,
        const char* save_path);
    void reset();

    bool tick(const SaveFlowInput& input);
    bool consume_handoff(save::StartContext* output);
    bool save_game(
        const save::StartContext& start,
        std::uint32_t play_seconds);

    SaveFlowPhase phase() const;
    const StartupRuntime& startup() const;
    const save::SaveBank& bank() const;
    save::FileSelectAction selected_action() const;
    std::size_t selected_slot() const;
    save::StorageResult last_storage_result() const;

private:
    static constexpr std::size_t kSavePathBytes = 256;

    bool set_save_path(const char* path);
    bool fail();
    void update_phase();

    StartupRuntime startup_ = {};
    save::SaveBank bank_ = {};
    save::FileSelectDecision decision_ = {};
    std::array<char, kSavePathBytes> save_path_ = {};
    save::StorageResult last_storage_result_ = save::StorageResult::NotFound;
    SaveFlowPhase phase_ = SaveFlowPhase::Error;
    bool initialized_ = false;
    bool decision_ready_ = false;
};

const char* save_flow_phase_name(SaveFlowPhase phase);

}  // namespace dusk::psp::startup

#endif
