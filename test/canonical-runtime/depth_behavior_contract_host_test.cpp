#include "dusk/psp/depth_behavior.hpp"

#include <cstdio>

namespace depth = dusk::psp::depth_behavior;

namespace {

depth::FixtureMetrics accepted_metrics() {
    depth::FixtureMetrics metrics = {};
    metrics.fixture_count = depth::kFixtureCount;
    metrics.near_far_ordered = true;
    metrics.reversed_depth_mapping_valid = true;
    metrics.order_invariant = true;
    for (std::uint32_t index = 0; index < depth::kFixtureCount; ++index) {
        metrics.fixtures[index].pixels_valid = true;
        metrics.fixtures[index].state_valid = true;
        metrics.fixtures[index].command_bytes = 1;
    }
    return metrics;
}

}  // namespace

int main() {
    const depth::FixtureMetrics accepted = accepted_metrics();
    if (!depth::fixture_metrics_accepted(accepted)) {
        std::fprintf(stderr, "accepted fixture summary rejected\n");
        return 1;
    }

    std::uint32_t negative_cases = 0;
    bool negative_cases_valid = true;
    const auto reject = [
        &negative_cases, &negative_cases_valid
    ](const depth::FixtureMetrics& metrics) {
        if (depth::fixture_metrics_accepted(metrics)) {
            negative_cases_valid = false;
        }
        ++negative_cases;
    };

    auto mutated = accepted;
    mutated.fixture_count = depth::kFixtureCount - 1;
    reject(mutated);
    mutated = accepted;
    mutated.failures = 1;
    reject(mutated);
    mutated = accepted;
    mutated.allocations = 1;
    reject(mutated);
    mutated = accepted;
    mutated.depth_state_leaks = 1;
    reject(mutated);
    mutated = accepted;
    mutated.render_target_state_leaks = 1;
    reject(mutated);
    mutated = accepted;
    mutated.near_far_ordered = false;
    reject(mutated);
    mutated = accepted;
    mutated.reversed_depth_mapping_valid = false;
    reject(mutated);
    mutated = accepted;
    mutated.order_invariant = false;
    reject(mutated);
    mutated = accepted;
    mutated.fixtures[3].pixels_valid = false;
    reject(mutated);
    mutated = accepted;
    mutated.fixtures[14].state_valid = false;
    reject(mutated);
    mutated = accepted;
    mutated.fixtures[0].command_bytes = 0;
    reject(mutated);

    if (!negative_cases_valid) {
        std::fprintf(stderr, "negative fixture summary accepted\n");
        return 1;
    }

    std::printf(
        "DEPTH_BEHAVIOR_CONTRACT_HOST_OK fixtures=%u negative_cases=%u\n",
        depth::kFixtureCount, negative_cases);
    return 0;
}
