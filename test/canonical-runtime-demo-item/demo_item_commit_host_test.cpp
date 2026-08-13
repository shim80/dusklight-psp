#include "d/actor/d_a_demo_item.h"
#include "dusk/psp/demo_item_runtime.hpp"
#include "dusk/psp/event_runtime.hpp"
#include "dusk/psp/interaction_runtime.hpp"
#include "dusk/psp/item_runtime.hpp"
#include "dusk/psp/model_runtime.hpp"
#include "dusk/psp/original_tbox_bridge.hpp"
#include "dusk/psp/playable_package.hpp"
#include "dusk/psp/process_runtime.hpp"
#include "dusk/psp/render_queue.hpp"
#include "dusk/psp/resource_manager.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

namespace compat = dusk::psp::compat;
namespace demo_item = dusk::psp::demo_item;
namespace events = dusk::psp::events;
namespace interaction = dusk::psp::interaction;
namespace items = dusk::psp::items;
namespace model = dusk::psp::model;
namespace playable = dusk::psp::playable;
namespace process = dusk::psp::process;
namespace render = dusk::psp::render;
namespace resources = dusk::psp::resources;

namespace {

constexpr std::int16_t kDemoItemCommitTestProcess = 0x7E00;
constexpr std::uint32_t kActorHeapSize =
    model::PspActorHeapArena::kCapacity;

void put_u16(
    std::uint8_t* bytes, std::uint32_t offset,
    std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_u32(
    std::uint8_t* bytes, std::uint32_t offset,
    std::uint32_t value) {
    for (std::uint32_t index = 0; index < 4; ++index) {
        bytes[offset + index] =
            static_cast<std::uint8_t>(value >> (index * 8));
    }
}

void put_f32(
    std::uint8_t* bytes, std::uint32_t offset,
    float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    put_u32(bytes, offset, bits);
}

void put_transform(
    std::uint8_t* bytes, std::uint32_t offset, float x) {
    put_f32(bytes, offset, x);
    put_f32(bytes, offset + 4, 0.0f);
    put_f32(bytes, offset + 8, 0.0f);
    put_f32(bytes, offset + 12, 0.0f);
    put_f32(bytes, offset + 16, 0.0f);
    put_f32(bytes, offset + 20, 0.0f);
    put_f32(bytes, offset + 24, 1.0f);
    put_f32(bytes, offset + 28, 1.0f);
    put_f32(bytes, offset + 32, 1.0f);
    put_f32(bytes, offset + 36, 1.0f);
}

bool make_heart_bck(
    std::uint8_t bytes[256], playable::PackageView* view) {
    std::memset(bytes, 0, 256);
    std::memcpy(bytes, "DPAN", 4);
    put_u16(bytes, 4, 1);
    put_u16(bytes, 6, 128);
    put_u32(bytes, 8, 256);
    put_u32(bytes, 16, 1);
    put_u32(bytes, 20, 1);
    put_u32(bytes, 24, 30);
    put_u32(bytes, 32, 128);
    put_u32(bytes, 36, 48);
    put_u32(bytes, 40, 176);
    put_u32(bytes, 128, 5);
    put_u32(bytes, 136, 2);
    put_u32(bytes, 140, 2);
    put_u32(bytes, 144, 1);
    put_u32(bytes, 152, 176);
    put_u32(bytes, 156, 80);
    put_transform(bytes, 176, 0.0f);
    put_transform(bytes, 216, 10.0f);
    put_u32(bytes, 12, playable::package_crc32(bytes, 256));
    return playable::validate_dpan(bytes, 256, view) ==
           playable::PackageError::Ok;
}

bool make_heart_brk(
    std::uint8_t bytes[256], playable::PackageView* view) {
    std::memset(bytes, 0, 256);
    std::memcpy(bytes, "DPBR", 4);
    put_u16(bytes, 4, 1);
    put_u16(bytes, 6, 128);
    put_u32(bytes, 8, 256);
    put_u32(bytes, 16, 1);
    put_u32(bytes, 32, 128);
    put_u32(bytes, 36, 32);

    put_u32(bytes, 128, 11);
    put_u32(bytes, 136, 2);
    put_u32(bytes, 140, 2);
    put_u32(bytes, 144, 1);
    put_u32(bytes, 148, 160);
    put_u32(bytes, 152, 168);
    put_u32(bytes, 156, 32);

    put_u16(bytes, 160, 3);
    bytes[162] = static_cast<std::uint8_t>(
        dusk::psp::animation::TevRegisterKind::Color);
    bytes[163] = 1;
    put_u32(bytes, 164, 0);

    put_f32(bytes, 168, 0.0f);
    put_f32(bytes, 172, 0.25f);
    put_f32(bytes, 176, 0.5f);
    put_f32(bytes, 180, 1.0f);
    put_f32(bytes, 184, 1.0f);
    put_f32(bytes, 188, 0.75f);
    put_f32(bytes, 192, 0.5f);
    put_f32(bytes, 196, 0.25f);

    put_u32(bytes, 12, playable::package_crc32(bytes, 256));
    return playable::validate_package(
               bytes, 256, "DPBR", view) ==
           playable::PackageError::Ok;
}

bool close(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

bool read_nothing(
    void* user, const char* path, void* output,
    std::uint32_t capacity, std::uint32_t* size) {
    (void)user;
    (void)path;
    (void)output;
    (void)capacity;
    (void)size;
    return false;
}

int test_create(void* instance) {
    auto* item = new (instance) daDitem_c();
    item->process_id = kDemoItemCommitTestProcess;
    item->parameters = dItemNo_KAKERA_HEART_e;
    item->m_itemNo = dItemNo_KAKERA_HEART_e;
    item->current.roomNo = 2;
    item->home.roomNo = 2;
    item->setAction(daDitem_c::ACTION_EVENT_e);
    return fopAcM_entrySolidHeap(
               item, CheckItemCreateHeap, kActorHeapSize) != 0
        ? cPhs_COMPLEATE_e : cPhs_ERROR_e;
}

int test_delete(void* instance) {
    static_cast<daDitem_c*>(instance)->~daDitem_c();
    return 1;
}

int test_execute(void* instance) {
    static_cast<daDitem_c*>(instance)->action();
    return 1;
}

int test_is_delete(void*) {
    return 1;
}

int test_draw(void* instance) {
    return static_cast<daDitem_c*>(instance)->DrawBase();
}

const actor_method_class kDemoItemCommitMethods = {
    reinterpret_cast<process_method_func>(test_create),
    reinterpret_cast<process_method_func>(test_delete),
    reinterpret_cast<process_method_func>(test_execute),
    reinterpret_cast<process_method_func>(test_is_delete),
    reinterpret_cast<process_method_func>(test_draw),
};

const actor_process_profile_definition kDemoItemCommitProfile = {
    fpcLy_CURRENT_e,
    7,
    fpcPi_CURRENT_e,
    kDemoItemCommitTestProcess,
    &g_fpcLf_Method.base,
    sizeof(daDitem_c),
    0,
    0,
    &g_fopAc_Method.base,
    fpcDwPi_Demo_Item_e,
    &kDemoItemCommitMethods,
    fopAcStts_UNK_0x40000_e | fopAcStts_NOPAUSE_e,
    fopAc_ACTOR_e,
    fopAc_CULLBOX_CUSTOM_e,
};

int fail(int code, const char* message) {
    std::fprintf(stderr, "DEMO_ITEM_COMMIT_HOST_FAILED code=%d %s\n",
                 code, message);
    return code;
}

}  // namespace

int main() {
    static process::PspProcessManager manager;
    static events::PspEventContext event_context;
    static interaction::PspInteractionContext interaction_context;
    static items::PspItemContext item_context;
    static resources::PspResourceManager resource_manager;
    static render::PspRenderQueue render_queue;
    static model::PspStaticModelRuntime model_runtime;

    static constexpr char kManifest[] =
        "DUSKLIGHT_RESOURCE_MANIFEST_V1\n"
        "unused|Scene|unused.bin|00000000\n";
    if (!resource_manager.initialize(
            ".", kManifest,
            static_cast<std::uint32_t>(sizeof(kManifest) - 1),
            read_nothing, nullptr)) {
        return fail(1, "resource-manager-init");
    }
    render_queue.initialize();
    if (!model_runtime.initialize(&resource_manager, &render_queue)) {
        return fail(2, "model-runtime-init");
    }
    model::bind_model_runtime(&model_runtime);

    std::uint8_t bck_bytes[256] = {};
    std::uint8_t brk_bytes[256] = {};
    playable::PackageView bck_package = {};
    playable::PackageView brk_package = {};
    if (!make_heart_bck(bck_bytes, &bck_package) ||
        !make_heart_brk(brk_bytes, &brk_package) ||
        !demo_item::configure_animation_resources(
            bck_package, 5, brk_package, 11)) {
        return fail(3, "animation-resource-bind");
    }

    manager.initialize();
    event_context.initialize();
    interaction_context.initialize();
    if (!item_context.initialize()) {
        return fail(4, "item-context-init");
    }
    process::bind_process_manager(&manager);
    if (!compat::bind_original_tbox_context(
            &manager, &event_context, &interaction_context, &item_context) ||
        !manager.register_profile(&kDemoItemCommitProfile)) {
        return fail(5, "runtime-bind");
    }

    const process::CreateInput input = {
        kDemoItemCommitTestProcess,
        dItemNo_KAKERA_HEART_e,
        {0.0f, 0.0f, 0.0f},
        {0, 0, 0},
        {1.0f, 1.0f, 1.0f},
        2,
        0,
        0,
        0,
        0,
    };
    process::ProcessHandle handle = {};
    if (!manager.create(input, &handle) ||
        !manager.handle_valid(handle)) {
        return fail(6, "process-create");
    }
    auto* item = static_cast<daDitem_c*>(manager.instance(handle));
    if (item == nullptr || item->chkDead() ||
        item->mAction != daDitem_c::ACTION_EVENT_e ||
        item->mpModel == nullptr || item->mpBckAnm == nullptr ||
        item->mpBrkAnm == nullptr ||
        item_context.quantity(dItemNo_KAKERA_HEART_e) != 0 ||
        model_runtime.metrics.actor_heap_peak == 0 ||
        model_runtime.metrics.actor_heap_overflows != 0) {
        return fail(7, "source-heap-create");
    }

    item->animPlay(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
    item->animEntry();
    const J3DModelData* presentation = demo_item::source_model_data();
    if (presentation == nullptr ||
        presentation->animation_resource_id() != 5 ||
        presentation->animation_joints() != 1 ||
        !close(presentation->animation_frame(), 1.0f) ||
        presentation->material_animation_resource_id() != 11 ||
        presentation->material_register_count() != 1 ||
        !close(presentation->material_animation_frame(), 1.0f) ||
        presentation->material_registers()[0].material != 3 ||
        presentation->material_registers()[0].index != 1 ||
        !close(presentation->material_registers()[0].rgba[0], 0.5f)) {
        return fail(8, "bck-brk-entry");
    }

    render_queue.begin_frame();
    if (!manager.draw_all() || render_queue.size() != 1 ||
        model_runtime.metrics.render_commands != 1) {
        return fail(9, "draw-submit");
    }

    // A normal process frame before message acknowledgement must not acquire.
    if (!manager.execute_all() ||
        item_context.quantity(dItemNo_KAKERA_HEART_e) != 0 ||
        item_context.metrics.acquisitions != 0) {
        return fail(10, "premature-acquire");
    }

    // Message acknowledgement only marks the source actor dead.
    item->dead();
    if (!item->chkDead() ||
        item_context.quantity(dItemNo_KAKERA_HEART_e) != 0) {
        return fail(11, "message-ack");
    }

    // The following normal source process pass owns execItemGet().
    if (!manager.execute_all() ||
        item->mAction != daDitem_c::ACTION_WAIT_LIGHT_END_e ||
        item_context.quantity(dItemNo_KAKERA_HEART_e) != 1 ||
        item_context.metrics.acquisitions != 1 ||
        item_context.metrics.quantity_added != 1) {
        return fail(12, "source-commit");
    }

    // A second normal frame proves actionEvent()/execItemGet() cannot re-enter.
    if (!manager.execute_all() ||
        item_context.quantity(dItemNo_KAKERA_HEART_e) != 1 ||
        item_context.metrics.acquisitions != 1 ||
        item_context.metrics.quantity_added != 1 ||
        item_context.metrics.invalid_requests != 0 ||
        manager.metrics.errors != 0 || model_runtime.metrics.errors != 0) {
        return fail(13, "duplicate-commit");
    }

    if (!manager.destroy(handle) || manager.active_count() != 0) {
        return fail(14, "process-destroy");
    }
    compat::unbind_original_tbox_context();
    process::unbind_process_manager();
    manager.shutdown();
    item_context.shutdown();
    interaction_context.shutdown();
    event_context.shutdown();
    demo_item::clear_animation_resources();
    model::unbind_model_runtime();
    model_runtime.shutdown();
    render_queue.shutdown();
    resource_manager.shutdown();

    std::puts(
        "DEMO_ITEM_COMMIT_HOST_OK item=0x21 source_owned=true "
        "commit_frames=2 acquisitions=1 duplicate_commits=0 "
        "heap=true bck=5 brk=11 tev=true draw_submit=1");
    return 0;
}
