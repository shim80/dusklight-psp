#include "dusk/psp/benchmark_stats.hpp"

#include <limits>

namespace dusk::psp::benchmark {
namespace {

std::uint32_t value(const FrameSample& sample, Field field) {
    switch (field) {
    case Field::CpuUpdate:
        return sample.cpu_update_us;
    case Field::Adapter:
        return sample.adapter_us;
    case Field::GuSubmission:
        return sample.gu_submission_us;
    case Field::GeSync:
        return sample.ge_sync_us;
    case Field::Presentation:
        return sample.presentation_us;
    case Field::FrameTotal:
        return sample.frame_total_us;
    case Field::CommandListBytes:
        return sample.command_list_bytes;
    case Field::CacheWritebackOperations:
        return sample.cache_writeback_operations;
    case Field::CacheInvalidateOperations:
        return sample.cache_invalidate_operations;
    }
    return 0;
}

void sift_down(
    std::uint32_t* values,
    std::uint32_t start,
    std::uint32_t end) {
    std::uint32_t root = start;
    while (root * 2 + 1 <= end) {
        std::uint32_t child = root * 2 + 1;
        if (child + 1 <= end && values[child] < values[child + 1]) {
            ++child;
        }
        if (values[root] >= values[child]) {
            return;
        }
        const std::uint32_t temporary = values[root];
        values[root] = values[child];
        values[child] = temporary;
        root = child;
    }
}

void heap_sort(std::uint32_t* values, std::uint32_t count) {
    if (count < 2) {
        return;
    }
    for (std::uint32_t start = count / 2; start > 0; --start) {
        sift_down(values, start - 1, count - 1);
    }
    for (std::uint32_t end = count - 1; end > 0; --end) {
        const std::uint32_t temporary = values[end];
        values[end] = values[0];
        values[0] = temporary;
        sift_down(values, 0, end - 1);
    }
}

std::uint32_t percentile_index(
    std::uint32_t count, std::uint32_t percentile) {
    const std::uint64_t rank =
        (static_cast<std::uint64_t>(count) * percentile + 99u) / 100u;
    return static_cast<std::uint32_t>(rank == 0 ? 0 : rank - 1);
}

std::uint32_t count_over(
    const SampleBuffer& buffer, std::uint32_t threshold) {
    std::uint32_t result = 0;
    for (std::uint32_t index = 0; index < buffer.count; ++index) {
        if (buffer.values[index].frame_total_us > threshold) {
            ++result;
        }
    }
    return result;
}

}  // namespace

bool collect(SampleBuffer* buffer, const FrameSample& sample) {
    if (buffer == nullptr || buffer->count >= kMeasuredFrames) {
        return false;
    }
    buffer->values[buffer->count++] = sample;
    return true;
}

std::uint32_t ticks_to_microseconds(
    std::uint64_t ticks, std::uint64_t ticks_per_second) {
    if (ticks_per_second == 0 ||
        ticks > std::numeric_limits<std::uint64_t>::max() / 1000000u) {
        return std::numeric_limits<std::uint32_t>::max();
    }
    const std::uint64_t result =
        (ticks * 1000000u + ticks_per_second / 2u) / ticks_per_second;
    return result > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(result);
}

Distribution summarize(
    const SampleBuffer& buffer,
    Field field,
    std::uint32_t* scratch,
    std::uint32_t scratch_count) {
    Distribution result = {};
    if (buffer.count == 0 ||
        buffer.count > kMeasuredFrames ||
        scratch == nullptr ||
        scratch_count < buffer.count) {
        return result;
    }
    std::uint64_t sum = 0;
    for (std::uint32_t index = 0; index < buffer.count; ++index) {
        scratch[index] = value(buffer.values[index], field);
        sum += scratch[index];
    }
    heap_sort(scratch, buffer.count);
    result.minimum = scratch[0];
    result.mean = sum / buffer.count;
    result.median =
        buffer.count % 2 == 0
            ? static_cast<std::uint32_t>(
                  (static_cast<std::uint64_t>(
                       scratch[buffer.count / 2 - 1]) +
                   scratch[buffer.count / 2]) /
                  2u)
            : scratch[buffer.count / 2];
    result.percentile_90 =
        scratch[percentile_index(buffer.count, 90)];
    result.percentile_95 =
        scratch[percentile_index(buffer.count, 95)];
    result.percentile_99 =
        scratch[percentile_index(buffer.count, 99)];
    result.maximum = scratch[buffer.count - 1];
    std::uint64_t absolute_deviation = 0;
    for (std::uint32_t index = 0; index < buffer.count; ++index) {
        const std::uint64_t current = scratch[index];
        absolute_deviation +=
            current >= result.mean
                ? current - result.mean
                : result.mean - current;
    }
    result.mean_absolute_deviation =
        absolute_deviation / buffer.count;
    result.sample_count = buffer.count;
    return result;
}

ScenarioStatistics calculate(
    const SampleBuffer& buffer,
    std::uint32_t* scratch,
    std::uint32_t scratch_count) {
    ScenarioStatistics result = {};
    result.cpu_update_us =
        summarize(buffer, Field::CpuUpdate, scratch, scratch_count);
    result.adapter_us =
        summarize(buffer, Field::Adapter, scratch, scratch_count);
    result.gu_submission_us =
        summarize(buffer, Field::GuSubmission, scratch, scratch_count);
    result.ge_sync_us =
        summarize(buffer, Field::GeSync, scratch, scratch_count);
    result.presentation_us =
        summarize(buffer, Field::Presentation, scratch, scratch_count);
    result.frame_total_us =
        summarize(buffer, Field::FrameTotal, scratch, scratch_count);
    result.command_list_bytes =
        summarize(buffer, Field::CommandListBytes, scratch, scratch_count);
    result.cache_writeback_operations = summarize(
        buffer, Field::CacheWritebackOperations, scratch, scratch_count);
    result.cache_invalidate_operations = summarize(
        buffer, Field::CacheInvalidateOperations, scratch, scratch_count);
    result.frames_over_16667_us = count_over(buffer, 16667);
    result.frames_over_33333_us = count_over(buffer, 33333);
    result.frames_over_50000_us = count_over(buffer, 50000);
    result.frames_over_66667_us = count_over(buffer, 66667);
    return result;
}

}  // namespace dusk::psp::benchmark
