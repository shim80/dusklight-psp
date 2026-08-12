#ifndef DUSK_PSP_OPAQUE_ORDER_HPP
#define DUSK_PSP_OPAQUE_ORDER_HPP

#include <cstdint>

namespace dusk::psp::opaque_order {

constexpr std::uint32_t kMaximumSubmissions = 256;
constexpr std::uint32_t kDeterministicSeed = 0x4455534bu;

enum class Variant : std::uint8_t {
    SourceOrder,
    ReverseOrder,
    DeterministicPermutation,
};

struct Submission {
    std::uint32_t source_index;
    std::uint16_t actor_id;
    std::uint16_t material_id;
    std::uint16_t shape_id;
    std::uint8_t source;
    bool depth_test;
    bool depth_write;
    bool source_order_dependent;
};

struct PlanMetrics {
    std::uint32_t submissions_total;
    std::uint32_t reorderable_submissions;
    std::uint32_t fixed_submissions;
    std::uint32_t seed;
    bool fixed_slots_preserved;
    bool complete_permutation;
};

constexpr bool reorderable(const Submission& submission) {
    return submission.depth_test && submission.depth_write &&
           !submission.source_order_dependent;
}

const char* variant_name(Variant variant);

bool build_plan(
    const Submission* submissions,
    std::uint32_t count,
    Variant variant,
    std::uint32_t seed,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    PlanMetrics* metrics);

}  // namespace dusk::psp::opaque_order

#endif
