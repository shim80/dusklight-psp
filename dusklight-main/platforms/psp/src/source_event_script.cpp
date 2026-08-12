#include "dusk/psp/source_event_script.hpp"

#include <cstring>

namespace dusk::psp::events {
namespace {
constexpr std::uint32_t kHeaderSize = 0x40;
constexpr std::uint32_t kEventSize = 0xB0;
constexpr std::uint32_t kStaffSize = 0x50;
constexpr std::uint32_t kCutSize = 0x50;
constexpr std::uint32_t kDataSize = 0x40;
constexpr std::uint8_t kActorEventBase = 2;
constexpr std::int32_t kMaxEventFlags = 0x2800;
SourceEventScript* g_source_event_script = nullptr;

bool bounded_equal(
    const std::uint8_t* bytes, std::uint32_t width,
    const char* wanted) {
    if (bytes == nullptr || wanted == nullptr) return false;
    for (std::uint32_t i = 0; i < width; ++i) {
        const char a = static_cast<char>(bytes[i]);
        const char b = wanted[i];
        if (a != b) return false;
        if (a == '\0') return true;
        if (b == '\0') return false;
    }
    return wanted[width] == '\0';
}
}  // namespace

bool SourceEventScript::range(
    std::uint32_t offset, std::uint32_t bytes) const {
    return data_ != nullptr && offset <= size_ && bytes <= size_ - offset;
}

std::uint16_t SourceEventScript::u16(std::uint32_t offset) const {
    if (!range(offset, 2)) return 0;
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data_[offset]) << 8) |
        data_[offset + 1]);
}

std::uint32_t SourceEventScript::u32(std::uint32_t offset) const {
    if (!range(offset, 4)) return 0;
    return (static_cast<std::uint32_t>(data_[offset]) << 24) |
           (static_cast<std::uint32_t>(data_[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(data_[offset + 2]) << 8) |
           static_cast<std::uint32_t>(data_[offset + 3]);
}

std::int32_t SourceEventScript::s32(std::uint32_t offset) const {
    return static_cast<std::int32_t>(u32(offset));
}

float SourceEventScript::f32(std::uint32_t offset) const {
    const std::uint32_t bits = u32(offset);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

const char* SourceEventScript::fixed_string(
    std::uint32_t offset, std::uint32_t width) const {
    if (!range(offset, width)) return nullptr;
    for (std::uint32_t i = 0; i < width; ++i) {
        if (data_[offset + i] == 0) {
            return reinterpret_cast<const char*>(data_ + offset);
        }
    }
    return nullptr;
}

bool SourceEventScript::fixed_equal(
    std::uint32_t offset, std::uint32_t width,
    const char* wanted) const {
    return range(offset, width) &&
           bounded_equal(data_ + offset, width, wanted);
}

void SourceEventScript::shutdown() {
    data_ = nullptr;
    size_ = 0;
    event_top_ = staff_top_ = cut_top_ = data_top_ = 0;
    float_top_ = integer_top_ = string_top_ = 0;
    event_count_ = staff_count_ = cut_count_ = data_count_ = 0;
    float_count_ = integer_count_ = string_count_ = 0;
    std::memset(flags_, 0, sizeof(flags_));
    for (auto& state : staff_state_) state = {};
    std::memset(float_cache_, 0, sizeof(float_cache_));
    std::memset(integer_cache_, 0, sizeof(integer_cache_));
    std::memset(vector_cache_, 0, sizeof(vector_cache_));
    source_actor_ = nullptr;
    active_event_id_ = -1;
    initialized_ = false;
    running_ = false;
    completed_ = false;
}

bool SourceEventScript::initialize(
    const void* source, std::uint32_t size) {
    shutdown();
    if (source == nullptr || size < kHeaderSize) return false;
    data_ = static_cast<const std::uint8_t*>(source);
    size_ = size;
    event_top_ = u32(0x00); event_count_ = s32(0x04);
    staff_top_ = u32(0x08); staff_count_ = s32(0x0C);
    cut_top_ = u32(0x10); cut_count_ = s32(0x14);
    data_top_ = u32(0x18); data_count_ = s32(0x1C);
    float_top_ = u32(0x20); float_count_ = s32(0x24);
    integer_top_ = u32(0x28); integer_count_ = s32(0x2C);
    string_top_ = u32(0x30); string_count_ = s32(0x34);
    if (event_count_ <= 0 || staff_count_ <= 0 || cut_count_ <= 0 ||
        data_count_ < 0 || float_count_ < 0 || integer_count_ < 0 ||
        string_count_ < 0 || staff_count_ > kMaxStaff ||
        float_count_ > kMaxFloatData || integer_count_ > kMaxIntegerData ||
        !range(event_top_, static_cast<std::uint32_t>(event_count_) * kEventSize) ||
        !range(staff_top_, static_cast<std::uint32_t>(staff_count_) * kStaffSize) ||
        !range(cut_top_, static_cast<std::uint32_t>(cut_count_) * kCutSize) ||
        !range(data_top_, static_cast<std::uint32_t>(data_count_) * kDataSize) ||
        !range(float_top_, static_cast<std::uint32_t>(float_count_) * 4u) ||
        !range(integer_top_, static_cast<std::uint32_t>(integer_count_) * 4u) ||
        !range(string_top_, static_cast<std::uint32_t>(string_count_))) {
        shutdown();
        return false;
    }
    for (std::int32_t i = 0; i < float_count_; ++i) {
        float_cache_[i] = f32(float_top_ + static_cast<std::uint32_t>(i) * 4u);
    }
    for (std::int32_t i = 0; i < integer_count_; ++i) {
        integer_cache_[i] = s32(integer_top_ + static_cast<std::uint32_t>(i) * 4u);
    }
    initialized_ = true;
    return true;
}

std::uint32_t SourceEventScript::event_offset(std::int32_t index) const {
    return event_top_ + static_cast<std::uint32_t>(index) * kEventSize;
}
std::uint32_t SourceEventScript::staff_offset(std::int32_t index) const {
    return staff_top_ + static_cast<std::uint32_t>(index) * kStaffSize;
}
std::uint32_t SourceEventScript::cut_offset(std::int32_t index) const {
    return cut_top_ + static_cast<std::uint32_t>(index) * kCutSize;
}
std::uint32_t SourceEventScript::data_offset(std::int32_t index) const {
    return data_top_ + static_cast<std::uint32_t>(index) * kDataSize;
}

std::int32_t SourceEventScript::event_index(
    std::int16_t composite_event_id) const {
    if (composite_event_id < 0) return -1;
    const std::uint16_t raw = static_cast<std::uint16_t>(composite_event_id);
    if ((raw >> 8) != kActorEventBase) return -1;
    const std::int32_t index = raw & 0xFFu;
    return index < event_count_ ? index : -1;
}

std::int16_t SourceEventScript::event_id(const char* wanted) const {
    if (!initialized_ || wanted == nullptr) return -1;
    for (std::int32_t i = 0; i < event_count_; ++i) {
        if (fixed_equal(event_offset(i), 32, wanted)) {
            return static_cast<std::int16_t>((kActorEventBase << 8) | i);
        }
    }
    return -1;
}

const char* SourceEventScript::event_name(
    std::int16_t composite_event_id) const {
    const std::int32_t index = event_index(composite_event_id);
    return index >= 0 ? fixed_string(event_offset(index), 32) : nullptr;
}

bool SourceEventScript::start(
    std::int16_t composite_event_id, const void* source_actor) {
    const std::int32_t index = event_index(composite_event_id);
    if (!initialized_ || index < 0 || source_actor == nullptr) return false;
    std::memset(flags_, 0, sizeof(flags_));
    for (auto& state : staff_state_) state = {};
    const std::uint32_t event = event_offset(index);
    const std::int32_t count = s32(event + 0x7C);
    if (count <= 0 || count > 20) return false;
    for (std::int32_t i = 0; i < count; ++i) {
        const std::int32_t staff = s32(event + 0x2C + static_cast<std::uint32_t>(i) * 4u);
        if (staff < 0 || staff >= staff_count_ || staff >= kMaxStaff) return false;
        const std::int32_t cut = s32(staff_offset(staff) + 0x30);
        if (cut < 0 || cut >= cut_count_) return false;
        staff_state_[staff].current_cut = cut;
        staff_state_[staff].active = true;
        staff_state_[staff].advanced = true;
    }
    source_actor_ = source_actor;
    active_event_id_ = composite_event_id;
    running_ = true;
    completed_ = false;
    return true;
}

bool SourceEventScript::reset() {
    if (!initialized_) return false;
    std::memset(flags_, 0, sizeof(flags_));
    for (auto& state : staff_state_) state = {};
    source_actor_ = nullptr;
    active_event_id_ = -1;
    running_ = false;
    completed_ = false;
    return true;
}

bool SourceEventScript::flag(std::int32_t index) const {
    if (index < 0 || index >= kMaxEventFlags) return false;
    return (flags_[static_cast<std::uint32_t>(index) >> 5u] &
            (1u << (static_cast<std::uint32_t>(index) & 31u))) != 0;
}

void SourceEventScript::set_flag(std::int32_t index) {
    if (index < 0 || index >= kMaxEventFlags) return;
    flags_[static_cast<std::uint32_t>(index) >> 5u] |=
        1u << (static_cast<std::uint32_t>(index) & 31u);
}

int SourceEventScript::cut_start_check(std::int32_t cut_index) const {
    if (cut_index < 0 || cut_index >= cut_count_) return 0;
    const std::uint32_t cut = cut_offset(cut_index);
    for (int i = 0; i < 3; ++i) {
        const std::int32_t required = s32(cut + 0x28 + static_cast<std::uint32_t>(i) * 4u);
        if (required == -1) return i == 0 ? -1 : 1;
        if (!flag(required)) return 0;
    }
    return 1;
}

bool SourceEventScript::advance_staff(std::int32_t id) {
    if (id < 0 || id >= staff_count_ || id >= kMaxStaff ||
        !staff_state_[id].active) return false;
    StaffState& state = staff_state_[id];
    state.advanced = false;
    if (state.current_cut < 0 || state.current_cut >= cut_count_) return false;
    const std::uint32_t current = cut_offset(state.current_cut);
    const std::int32_t current_flag = s32(current + 0x34);
    const std::int32_t next = s32(current + 0x3C);
    if (!flag(current_flag) || next < 0 || next >= cut_count_) return false;
    const int start = cut_start_check(next);
    if (start != -1 && start != 1) return false;
    if (start == 1) set_flag(current_flag);
    state.current_cut = next;
    state.advanced = true;
    return true;
}

bool SourceEventScript::finish_check() const {
    const std::int32_t index = event_index(active_event_id_);
    if (!running_ || index < 0) return false;
    const std::uint32_t event = event_offset(index);
    for (int i = 0; i < 3; ++i) {
        const std::int32_t required = s32(event + 0x88 + static_cast<std::uint32_t>(i) * 4u);
        if (required == -1) return true;
        if (!flag(required)) return false;
    }
    return true;
}

void SourceEventScript::tick() {
    if (!running_) return;
    const std::int32_t index = event_index(active_event_id_);
    if (index < 0) return;
    const std::uint32_t event = event_offset(index);
    const std::int32_t count = s32(event + 0x7C);
    for (std::int32_t i = 0; i < count; ++i) {
        advance_staff(s32(event + 0x2C + static_cast<std::uint32_t>(i) * 4u));
    }
    if (finish_check()) {
        running_ = false;
        completed_ = true;
    }
}

int SourceEventScript::staff_id(
    const char* wanted, int tag_id) const {
    if (!running_ || wanted == nullptr) return -1;
    const std::int32_t index = event_index(active_event_id_);
    if (index < 0) return -1;
    const std::uint32_t event = event_offset(index);
    const std::int32_t count = s32(event + 0x7C);
    for (std::int32_t i = 0; i < count; ++i) {
        const std::int32_t staff = s32(event + 0x2C + static_cast<std::uint32_t>(i) * 4u);
        const std::uint32_t entry = staff_offset(staff);
        const std::int32_t tag = s32(entry + 0x20);
        const bool name = fixed_equal(entry, 8, wanted) ||
            (std::strcmp(wanted, "Alink") == 0 && fixed_equal(entry, 8, "Link"));
        if (name && (tag_id < 0 || tag_id == tag)) return staff;
    }
    return -1;
}

const char* SourceEventScript::current_cut_name(int id) const {
    if (!running_ || id < 0 || id >= staff_count_ || id >= kMaxStaff ||
        !staff_state_[id].active) return nullptr;
    const std::int32_t cut = staff_state_[id].current_cut;
    return cut >= 0 && cut < cut_count_ ? fixed_string(cut_offset(cut), 32) : nullptr;
}

int SourceEventScript::action_index(
    int id, const char* const* actions,
    int action_count, bool prefix_match) const {
    const char* current = current_cut_name(id);
    if (current == nullptr || actions == nullptr || action_count <= 0) return -1;
    for (int i = 0; i < action_count; ++i) {
        if (actions[i] == nullptr) continue;
        if ((!prefix_match && std::strcmp(actions[i], current) == 0) ||
            (prefix_match && std::strncmp(current, actions[i], std::strlen(actions[i])) == 0)) {
            return i;
        }
    }
    return -1;
}

bool SourceEventScript::staff_advanced(int id) const {
    return running_ && id >= 0 && id < staff_count_ && id < kMaxStaff &&
           staff_state_[id].active && staff_state_[id].advanced;
}

std::int32_t SourceEventScript::find_data(int id, const char* name) const {
    if (!running_ || name == nullptr || id < 0 || id >= staff_count_ ||
        id >= kMaxStaff || !staff_state_[id].active) return -1;
    const std::int32_t cut = staff_state_[id].current_cut;
    if (cut < 0 || cut >= cut_count_) return -1;
    std::int32_t entry = s32(cut_offset(cut) + 0x38);
    for (std::uint16_t guard = 0; entry >= 0 && entry < data_count_ && guard < 1024; ++guard) {
        const std::uint32_t offset = data_offset(entry);
        if (fixed_equal(offset, 32, name)) return entry;
        entry = s32(offset + 0x30);
    }
    return -1;
}

int SourceEventScript::substance_count(int id, const char* name) const {
    const std::int32_t entry = find_data(id, name);
    return entry >= 0 ? s32(data_offset(entry) + 0x2C) : 0;
}

const std::int32_t* SourceEventScript::integer_data(
    int id, const char* name) const {
    const std::int32_t entry = find_data(id, name);
    if (entry < 0) return nullptr;
    const std::uint32_t offset = data_offset(entry);
    if (s32(offset + 0x24) != 3) return nullptr;
    const std::int32_t index = s32(offset + 0x28);
    const std::int32_t count = s32(offset + 0x2C);
    return index >= 0 && count > 0 && index + count <= integer_count_
        ? &integer_cache_[index] : nullptr;
}

const float* SourceEventScript::float_data(
    int id, const char* name) const {
    const std::int32_t entry = find_data(id, name);
    if (entry < 0) return nullptr;
    const std::uint32_t offset = data_offset(entry);
    if (s32(offset + 0x24) != 0) return nullptr;
    const std::int32_t index = s32(offset + 0x28);
    const std::int32_t count = s32(offset + 0x2C);
    return index >= 0 && count > 0 && index + count <= float_count_
        ? &float_cache_[index] : nullptr;
}

const SourceEventVec3* SourceEventScript::vector_data(
    int id, const char* name) const {
    const std::int32_t entry = find_data(id, name);
    if (entry < 0) return nullptr;
    const std::uint32_t offset = data_offset(entry);
    if (s32(offset + 0x24) != 1) return nullptr;
    const std::int32_t index = s32(offset + 0x28);
    const std::int32_t count = s32(offset + 0x2C);
    if (index < 0 || count < 1 || index + count * 3 > float_count_ ||
        index >= kMaxFloatData) return nullptr;
    vector_cache_[index] = {
        float_cache_[index], float_cache_[index + 1], float_cache_[index + 2]};
    return &vector_cache_[index];
}

const char* SourceEventScript::string_data(
    int id, const char* name) const {
    const std::int32_t entry = find_data(id, name);
    if (entry < 0) return nullptr;
    const std::uint32_t offset = data_offset(entry);
    if (s32(offset + 0x24) != 4) return nullptr;
    const std::int32_t index = s32(offset + 0x28);
    if (index < 0 || index >= string_count_) return nullptr;
    const std::uint32_t absolute = string_top_ + static_cast<std::uint32_t>(index);
    if (!range(absolute, 1)) return nullptr;
    const std::uint32_t remaining = static_cast<std::uint32_t>(string_count_ - index);
    for (std::uint32_t i = 0; i < remaining; ++i) {
        if (data_[absolute + i] == 0) return reinterpret_cast<const char*>(data_ + absolute);
    }
    return nullptr;
}

bool SourceEventScript::cut_end(int id) {
    if (!running_ || id < 0 || id >= staff_count_ || id >= kMaxStaff ||
        !staff_state_[id].active) return false;
    const std::int32_t cut = staff_state_[id].current_cut;
    if (cut < 0 || cut >= cut_count_) return false;
    set_flag(s32(cut_offset(cut) + 0x34));
    return true;
}

bool SourceEventScript::end_check(
    std::int16_t composite_event_id) const {
    return completed_ && composite_event_id == active_event_id_;
}

bool SourceEventScript::end_check(const char* wanted) const {
    if (!completed_ || wanted == nullptr) return false;
    const char* active = event_name(active_event_id_);
    return active != nullptr && std::strcmp(active, wanted) == 0;
}

void bind_source_event_script(SourceEventScript* script) {
    g_source_event_script = script;
}
void unbind_source_event_script() { g_source_event_script = nullptr; }
SourceEventScript* source_event_script() { return g_source_event_script; }

}  // namespace dusk::psp::events
