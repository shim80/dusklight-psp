#ifndef DUSK_PSP_SOURCE_EVENT_SCRIPT_HPP
#define DUSK_PSP_SOURCE_EVENT_SCRIPT_HPP

#include <cstdint>

namespace dusk::psp::events {

struct SourceEventVec3 {
    float x;
    float y;
    float z;
};

class SourceEventScript {
public:
    static constexpr std::uint16_t kMaxStaff = 256;
    static constexpr std::uint16_t kFlagWords = 320;
    static constexpr std::uint16_t kMaxFloatData = 2048;
    static constexpr std::uint16_t kMaxIntegerData = 1024;

    bool initialize(const void* data, std::uint32_t size);
    void shutdown();

    std::int16_t event_id(const char* event_name) const;
    const char* event_name(std::int16_t composite_event_id) const;
    bool start(std::int16_t composite_event_id, const void* source_actor);
    bool reset();
    void tick();

    int staff_id(const char* staff_name, int tag_id) const;
    int action_index(
        int staff_id, const char* const* actions,
        int action_count, bool prefix_match) const;
    bool staff_advanced(int staff_id) const;
    const char* current_cut_name(int staff_id) const;
    const std::int32_t* integer_data(int staff_id, const char* name) const;
    const float* float_data(int staff_id, const char* name) const;
    const SourceEventVec3* vector_data(int staff_id, const char* name) const;
    const char* string_data(int staff_id, const char* name) const;
    int substance_count(int staff_id, const char* name) const;
    bool cut_end(int staff_id);
    bool end_check(std::int16_t composite_event_id) const;
    bool end_check(const char* event_name) const;

    bool initialized() const { return initialized_; }
    bool running() const { return running_; }
    bool completed() const { return completed_; }
    std::int16_t active_event_id() const { return active_event_id_; }
    const void* source_actor() const { return source_actor_; }

private:
    struct StaffState {
        std::int32_t current_cut = -1;
        bool active = false;
        bool advanced = false;
    };

    std::uint16_t u16(std::uint32_t offset) const;
    std::uint32_t u32(std::uint32_t offset) const;
    std::int32_t s32(std::uint32_t offset) const;
    float f32(std::uint32_t offset) const;
    bool range(std::uint32_t offset, std::uint32_t bytes) const;
    const char* fixed_string(std::uint32_t offset, std::uint32_t width) const;
    bool fixed_equal(
        std::uint32_t offset, std::uint32_t width,
        const char* wanted) const;

    std::int32_t event_index(std::int16_t composite_event_id) const;
    std::uint32_t event_offset(std::int32_t index) const;
    std::uint32_t staff_offset(std::int32_t index) const;
    std::uint32_t cut_offset(std::int32_t index) const;
    std::uint32_t data_offset(std::int32_t index) const;
    bool flag(std::int32_t index) const;
    void set_flag(std::int32_t index);
    int cut_start_check(std::int32_t cut_index) const;
    bool advance_staff(std::int32_t staff_id);
    bool finish_check() const;
    std::int32_t find_data(int staff_id, const char* name) const;

    const std::uint8_t* data_ = nullptr;
    std::uint32_t size_ = 0;
    std::uint32_t event_top_ = 0;
    std::uint32_t staff_top_ = 0;
    std::uint32_t cut_top_ = 0;
    std::uint32_t data_top_ = 0;
    std::uint32_t float_top_ = 0;
    std::uint32_t integer_top_ = 0;
    std::uint32_t string_top_ = 0;
    std::int32_t event_count_ = 0;
    std::int32_t staff_count_ = 0;
    std::int32_t cut_count_ = 0;
    std::int32_t data_count_ = 0;
    std::int32_t float_count_ = 0;
    std::int32_t integer_count_ = 0;
    std::int32_t string_count_ = 0;
    std::uint32_t flags_[kFlagWords] = {};
    StaffState staff_state_[kMaxStaff] = {};
    float float_cache_[kMaxFloatData] = {};
    std::int32_t integer_cache_[kMaxIntegerData] = {};
    mutable SourceEventVec3 vector_cache_[kMaxFloatData] = {};
    const void* source_actor_ = nullptr;
    std::int16_t active_event_id_ = -1;
    bool initialized_ = false;
    bool running_ = false;
    bool completed_ = false;
};

void bind_source_event_script(SourceEventScript* script);
void unbind_source_event_script();
SourceEventScript* source_event_script();

}  // namespace dusk::psp::events

#endif
