#include "dusk/psp/parity_trace.hpp"

#include <cstdio>
#include <cstring>

namespace parity = dusk::psp::parity;

namespace {

struct Capture {
    char line[parity::kTraceLineCapacity];
    std::size_t size;
};

bool capture(void* user, const char* line, std::size_t size) {
    auto* output = static_cast<Capture*>(user);
    if (size >= sizeof(output->line)) {
        return false;
    }
    std::memcpy(output->line, line, size);
    output->line[size] = '\0';
    output->size = size;
    return true;
}

}  // namespace

int main() {
    const parity::ParitySceneId scene = {
        14, 0x12345678u, 1, 13, 2,
    };
    const parity::ParityActorId actor = {
        0x12345678u, 1, 13, 0xabcdef01u, 7, 88, 3,
    };
    const parity::ParityModelId model = {
        actor, 0x76543210u, 4, 1, 2,
    };
    char actor_text[128] = {};
    char model_text[192] = {};
    if (!parity::format_id(actor, actor_text, sizeof(actor_text)) ||
        !parity::format_id(model, model_text, sizeof(model_text)) ||
        parity::stable_hash(actor) == 0 ||
        parity::stable_hash(model) == parity::stable_hash(actor)) {
        return 1;
    }
    Capture output = {};
    parity::DtrcV3Writer writer = {};
    if (!writer.initialize(capture, &output)) {
        return 2;
    }
    const parity::EventHeader header = {
        "sha256:0000000000000000000000000000000000000000000000000000000000000000",
        "host-run", "f_sp108_first_frame", 1, 1, scene,
        "F_SP108", 1, 13, 0xabcdef01u, 7, 0x33333333u,
        88, 88, 3, 4, 1,
        parity::LifecycleCheckpoint::FramePresent,
    };
    if (!writer.emit(
            header, parity::EventType::ActorTransform,
            "{\"actor_id\":\"actor:12345678:1:13:abcdef01:7:88:3\","
            "\"position\":[1,2,3]}") ||
        !writer.emit(
            header, parity::EventType::JointReference,
            "{\"joint\":\"left_foot\",\"joint_index\":21,"
            "\"position\":[1,2,3]}") ||
        !writer.emit(
            header, parity::EventType::JointReference,
            "{\"joint\":\"right_foot\",\"joint_index\":26,"
            "\"position\":[4,5,6]}") ||
        !writer.emit(
            header, parity::EventType::FloorContact,
            "{\"actor_id\":\"actor:12345678:1:13:abcdef01:7:88:3\","
            "\"floor\":2}") ||
        writer.events_written() != 4 || !writer.healthy() ||
        std::strstr(output.line, "\"schema_version\":3") == nullptr ||
        std::strstr(output.line, "\"schema_revision\":1") == nullptr ||
        std::strstr(output.line, "\"build_identity\":\"sha256:") == nullptr ||
        std::strstr(
            output.line,
            "\"lifecycle_checkpoint\":\"frame_present\"") == nullptr ||
        std::strstr(output.line, "\"floor_contact\"") == nullptr ||
        writer.emit(
            header, static_cast<parity::EventType>(255), "{}") ||
        writer.dropped_events() != 1 || writer.healthy()) {
        return 3;
    }
    std::puts(
        "PARITY_TRACE_HOST_OK stable_ids=true dtrc_v3_1=true "
        "pointer_ids=false semantic_events=3 negatives=1");
    return 0;
}
