#ifndef DUSK_PSP_ACTOR_PROFILE_REGISTRY_HPP
#define DUSK_PSP_ACTOR_PROFILE_REGISTRY_HPP

#include "dusk/psp/process_runtime.hpp"

#include <cstdint>

namespace dusk::psp::compat {

bool register_all_original_actor_profiles(
    process::PspProcessManager* manager);
std::uint16_t original_actor_profile_count();

}  // namespace dusk::psp::compat

#endif
