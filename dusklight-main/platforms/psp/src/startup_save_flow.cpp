#include "dusk/psp/startup_save_flow.hpp"

namespace dusk::psp::startup {

bool StartupSaveFlow::set_save_path(const char* path) {
    save_path_.fill('\0');
    if (path == nullptr || path[0] == '\0') {
        return false;
    }
    std::size_t index = 0;
    for (; index + 1 < save_path_.size() && path[index] != '\0'; ++index) {
        save_path_[index] = path[index];
    }
    if (path[index] != '\0') {
        save_path_.fill('\0');
        return false;
    }
    return true;
}

void StartupSaveFlow::reset() {
    startup_.reset();
    bank_.reset();
    decision_ = {};
    save_path_.fill('\0');
    last_storage_result_ = save::StorageResult::NotFound;
    phase_ = SaveFlowPhase::Error;
    initialized_ = false;
    decision_ready_ = false;
}

bool StartupSaveFlow::fail() {
    phase_ = SaveFlowPhase::Error;
    initialized_ = false;
    return false;
}

bool StartupSaveFlow::initialize(
    const PackageView& package,
    std::uint32_t available_capabilities,
    const char* save_path) {
    reset();
    if (!set_save_path(save_path)) {
        last_storage_result_ = save::StorageResult::ReadError;
        return fail();
    }

    last_storage_result_ = save::load_bank_file(save_path_.data(), &bank_);
    if (last_storage_result_ == save::StorageResult::NotFound) {
        bank_.reset();
    } else if (last_storage_result_ != save::StorageResult::Ok) {
        return fail();
    }

    if (!startup_.initialize(package, available_capabilities)) {
        return fail();
    }
    initialized_ = true;
    update_phase();
    return phase_ != SaveFlowPhase::Error;
}

void StartupSaveFlow::update_phase() {
    if (!initialized_) {
        phase_ = SaveFlowPhase::Error;
        return;
    }
    if (decision_ready_ &&
        (startup_.finished() ||
         startup_.current_segment() == Segment::UnsupportedGameplay)) {
        phase_ = SaveFlowPhase::HandoffReady;
        return;
    }
    if (startup_.current_segment() == Segment::FileSelect) {
        phase_ = SaveFlowPhase::FileSelect;
        return;
    }
    phase_ = decision_ready_
        ? SaveFlowPhase::Transition
        : SaveFlowPhase::Startup;
}

bool StartupSaveFlow::tick(const SaveFlowInput& input) {
    if (!initialized_ || phase_ == SaveFlowPhase::Error) {
        return false;
    }
    if (phase_ == SaveFlowPhase::HandoffReady ||
        phase_ == SaveFlowPhase::Gameplay) {
        return true;
    }

    if (startup_.current_segment() == Segment::FileSelect) {
        if (startup_.blocked()) {
            return fail();
        }
        const save::SaveBank before = bank_;
        save::FileSelectRuntime file_select(&bank_);
        save::FileSelectDecision decision = {};
        if (!file_select.tick(
                {input.up, input.down, input.confirm, input.start},
                &decision)) {
            return fail();
        }
        if (decision.action == save::FileSelectAction::None) {
            update_phase();
            return true;
        }

        if (decision.action == save::FileSelectAction::NewGame) {
            last_storage_result_ =
                save::store_bank_file(save_path_.data(), bank_);
            if (last_storage_result_ != save::StorageResult::Ok) {
                bank_ = before;
                return fail();
            }
        }

        decision_ = decision;
        decision_ready_ = true;
        if (!startup_.tick(
                {input.confirm, input.start},
                input.resources_ready,
                input.source_event_complete)) {
            return fail();
        }
        if (startup_.current_segment() == Segment::FileSelect) {
            return fail();
        }
        update_phase();
        return true;
    }

    if (!startup_.tick(
            {input.confirm, input.start},
            input.resources_ready,
            input.source_event_complete)) {
        return fail();
    }
    update_phase();
    return true;
}

bool StartupSaveFlow::consume_handoff(save::StartContext* output) {
    if (!initialized_ || output == nullptr ||
        phase_ != SaveFlowPhase::HandoffReady || !decision_ready_) {
        return false;
    }
    *output = decision_.start;
    phase_ = SaveFlowPhase::Gameplay;
    return true;
}

bool StartupSaveFlow::save_game(
    const save::StartContext& start,
    std::uint32_t play_seconds) {
    if (!initialized_ || phase_ != SaveFlowPhase::Gameplay) {
        return false;
    }
    const std::size_t slot = bank_.selected_slot();
    const save::SaveBank before = bank_;
    if (!bank_.update_slot(slot, start, play_seconds)) {
        return false;
    }
    last_storage_result_ = save::store_bank_file(save_path_.data(), bank_);
    if (last_storage_result_ != save::StorageResult::Ok) {
        bank_ = before;
        return fail();
    }
    return true;
}

SaveFlowPhase StartupSaveFlow::phase() const {
    return phase_;
}

const StartupRuntime& StartupSaveFlow::startup() const {
    return startup_;
}

const save::SaveBank& StartupSaveFlow::bank() const {
    return bank_;
}

save::FileSelectAction StartupSaveFlow::selected_action() const {
    return decision_ready_
        ? decision_.action
        : save::FileSelectAction::None;
}

std::size_t StartupSaveFlow::selected_slot() const {
    return bank_.selected_slot();
}

save::StorageResult StartupSaveFlow::last_storage_result() const {
    return last_storage_result_;
}

const char* save_flow_phase_name(SaveFlowPhase phase) {
    switch (phase) {
    case SaveFlowPhase::Startup: return "startup";
    case SaveFlowPhase::FileSelect: return "file_select";
    case SaveFlowPhase::Transition: return "transition";
    case SaveFlowPhase::HandoffReady: return "handoff_ready";
    case SaveFlowPhase::Gameplay: return "gameplay";
    case SaveFlowPhase::Error: return "error";
    }
    return "unknown";
}

}  // namespace dusk::psp::startup
