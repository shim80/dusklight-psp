#include "dusk/psp/opaque_order.hpp"

namespace dusk::psp::opaque_order {
namespace {

std::uint32_t next_random(std::uint32_t* state) {
    std::uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

bool validate_plan(
    const Submission* submissions,
    std::uint32_t count,
    const std::uint16_t* output,
    PlanMetrics* metrics) {
    bool seen[kMaximumSubmissions] = {};
    bool fixed_preserved = true;
    bool complete = true;
    for (std::uint32_t slot = 0; slot < count; ++slot) {
        const std::uint32_t source = output[slot];
        if (source >= count || seen[source]) {
            complete = false;
            continue;
        }
        seen[source] = true;
        if (!reorderable(submissions[slot]) && source != slot) {
            fixed_preserved = false;
        }
        if (reorderable(submissions[slot]) !=
            reorderable(submissions[source])) {
            fixed_preserved = false;
        }
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        complete = complete && seen[index];
    }
    metrics->fixed_slots_preserved = fixed_preserved;
    metrics->complete_permutation = complete;
    return fixed_preserved && complete;
}

}  // namespace

const char* variant_name(Variant variant) {
    switch (variant) {
    case Variant::SourceOrder: return "source_order";
    case Variant::ReverseOrder: return "reverse_order";
    case Variant::DeterministicPermutation:
        return "deterministic_permutation";
    }
    return "invalid";
}

bool build_plan(
    const Submission* submissions,
    std::uint32_t count,
    Variant variant,
    std::uint32_t seed,
    std::uint16_t* output,
    std::uint32_t output_capacity,
    PlanMetrics* metrics) {
    if (submissions == nullptr || output == nullptr || metrics == nullptr ||
        count == 0 || count > kMaximumSubmissions ||
        output_capacity < count || seed != kDeterministicSeed) {
        return false;
    }
    *metrics = {};
    metrics->submissions_total = count;
    metrics->seed = seed;
    std::uint16_t eligible_slots[kMaximumSubmissions] = {};
    std::uint16_t eligible[kMaximumSubmissions] = {};
    std::uint32_t eligible_count = 0;
    for (std::uint32_t index = 0; index < count; ++index) {
        output[index] = static_cast<std::uint16_t>(index);
        if (reorderable(submissions[index])) {
            eligible_slots[eligible_count] =
                static_cast<std::uint16_t>(index);
            eligible[eligible_count++] = static_cast<std::uint16_t>(index);
        }
    }
    metrics->reorderable_submissions = eligible_count;
    metrics->fixed_submissions = count - eligible_count;

    if (variant == Variant::ReverseOrder) {
        for (std::uint32_t index = 0; index < eligible_count; ++index) {
            output[eligible_slots[index]] =
                eligible_slots[eligible_count - 1u - index];
        }
    } else if (variant == Variant::DeterministicPermutation) {
        std::uint32_t state = seed;
        for (std::uint32_t remaining = eligible_count;
             remaining > 1; --remaining) {
            const std::uint32_t swap_index =
                next_random(&state) % remaining;
            const std::uint16_t temporary = eligible[remaining - 1u];
            eligible[remaining - 1u] = eligible[swap_index];
            eligible[swap_index] = temporary;
        }
        bool changed = eligible_count < 2;
        for (std::uint32_t index = 0; index < eligible_count; ++index) {
            changed = changed || eligible[index] != eligible_slots[index];
        }
        if (!changed) {
            const std::uint16_t first = eligible[0];
            for (std::uint32_t index = 1; index < eligible_count; ++index) {
                eligible[index - 1] = eligible[index];
            }
            eligible[eligible_count - 1] = first;
        }
        for (std::uint32_t index = 0; index < eligible_count; ++index) {
            output[eligible_slots[index]] = eligible[index];
        }
    } else if (variant != Variant::SourceOrder) {
        return false;
    }
    return validate_plan(submissions, count, output, metrics);
}

}  // namespace dusk::psp::opaque_order
