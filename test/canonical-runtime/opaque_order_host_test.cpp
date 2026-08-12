#include "dusk/psp/opaque_order.hpp"

#include <cstdio>

namespace order = dusk::psp::opaque_order;

int main() {
    constexpr order::Submission submissions[] = {
        {0, 0, 10, 10, 0, true, true, false},
        {1, 0, 11, 11, 0, true, false, false},
        {2, 0, 12, 12, 0, true, true, false},
        {3, 1, 20, 20, 1, true, true, true},
        {4, 253, 30, 30, 2, true, true, false},
        {5, 253, 31, 31, 2, true, true, false},
    };
    constexpr std::uint32_t count =
        sizeof(submissions) / sizeof(submissions[0]);
    std::uint16_t source[count] = {};
    std::uint16_t reversed[count] = {};
    std::uint16_t permuted[count] = {};
    order::PlanMetrics metrics = {};
    if (!order::build_plan(
            submissions, count, order::Variant::SourceOrder,
            order::kDeterministicSeed, source, count, &metrics) ||
        metrics.reorderable_submissions != 4 ||
        metrics.fixed_submissions != 2 ||
        !metrics.fixed_slots_preserved || !metrics.complete_permutation) {
        return 1;
    }
    if (!order::build_plan(
            submissions, count, order::Variant::ReverseOrder,
            order::kDeterministicSeed, reversed, count, &metrics) ||
        reversed[0] != 5 || reversed[1] != 1 || reversed[2] != 4 ||
        reversed[3] != 3 || reversed[4] != 2 || reversed[5] != 0) {
        return 2;
    }
    if (!order::build_plan(
            submissions, count,
            order::Variant::DeterministicPermutation,
            order::kDeterministicSeed, permuted, count, &metrics) ||
        permuted[1] != 1 || permuted[3] != 3) {
        return 3;
    }
    std::uint16_t repeated[count] = {};
    if (!order::build_plan(
            submissions, count,
            order::Variant::DeterministicPermutation,
            order::kDeterministicSeed, repeated, count, &metrics)) {
        return 4;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        if (repeated[index] != permuted[index]) return 5;
    }

    std::uint32_t negative_cases = 0;
    const auto rejects = [&](bool accepted) {
        ++negative_cases;
        return !accepted;
    };
    if (!rejects(order::build_plan(
            submissions, count,
            order::Variant::DeterministicPermutation,
            0x12345678u, repeated, count, &metrics)) ||
        !rejects(order::build_plan(
            nullptr, count, order::Variant::SourceOrder,
            order::kDeterministicSeed, repeated, count, &metrics)) ||
        !rejects(order::build_plan(
            submissions, count, order::Variant::SourceOrder,
            order::kDeterministicSeed, repeated, count - 1, &metrics))) {
        return 6;
    }
    std::printf(
        "OPAQUE_ORDER_HOST_OK submissions=%u reorderable=4 fixed=2 "
        "seed=%08x negative_cases=%u\n",
        count, order::kDeterministicSeed, negative_cases);
    return 0;
}
