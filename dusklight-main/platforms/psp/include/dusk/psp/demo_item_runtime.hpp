#ifndef DUSK_PSP_DEMO_ITEM_RUNTIME_HPP
#define DUSK_PSP_DEMO_ITEM_RUNTIME_HPP

#include "dusk/psp/playable_package.hpp"

#include <cstdint>

class J3DModelData;

namespace dusk::psp::demo_item {

bool configure_animation_resources(
    const playable::PackageView& bck_package,
    std::uint32_t bck_resource_id,
    const playable::PackageView& brk_package,
    std::uint32_t brk_resource_id);
void clear_animation_resources();
const J3DModelData* source_model_data();

}  // namespace dusk::psp::demo_item

#endif
