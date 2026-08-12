#ifndef DUSK_PSP_BENCHMARK_METRICS_HPP
#define DUSK_PSP_BENCHMARK_METRICS_HPP

#include <cstdint>

namespace dusk::psp::benchmark {

bool metrics_schema_complete(
    const char* text, std::uint32_t size);

}  // namespace dusk::psp::benchmark

#endif
