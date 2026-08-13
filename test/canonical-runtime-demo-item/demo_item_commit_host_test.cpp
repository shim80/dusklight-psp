#include "d/actor/d_a_demo_item.h"
#include "dusk/psp/event_runtime.hpp"
#include "dusk/psp/interaction_runtime.hpp"
#include "dusk/psp/item_runtime.hpp"
#include "dusk/psp/original_tbox_bridge.hpp"
#include "dusk/psp/process_runtime.hpp"

#include <cstdio>
#include <new>

namespace compat = dusk::psp::compat;
namespace events = dusk::psp::events;
namespace interaction = dusk::psp::interaction;
namespace items = dusk::psp::items;
namespace process = dusk::psp::process;

namespace {

constexpr std::int16_t kDemoItemCommitTestProcess = 0x7E00;

int test_create(void* instance) {
    auto* item = new (instance) daDitem_c();
    item->process_id = kDemoItemCommitTestProcess;
    item->parameters = dItemNo_KAKERA_HEART_e;
    item->m_itemNo = dItemNo_KAKERA_HEART_e;
    item->current.roomNo = 2;
    item->home.roomNo = 2;
    item->setAction(daDitem_c::ACTION_EVENT_e);
    return cPhs_COMPLEATE_e;
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

int test_draw(void*) {
    return 1;
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

}  // namespace

int main() {
    process::PspProcessManager manager;
    events::PspEventContext event_context;
    interaction::PspInteractionContext interaction_context;
    items::PspItemContext item_context;

    manager.initialize();
    event_context.initialize();
    interaction_context.initialize();
    if (!item_context.initialize()) {
        return 1;
    }
    process::bind_process_manager(&manager);
    if (!compat::bind_original_tbox_context(
            &manager, &event_context, &interaction_context, &item_context) ||
        !manager.register_profile(&kDemoItemCommitProfile)) {
        return 2;
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
        return 3;
    }
    auto* item = static_cast<daDitem_c*>(manager.instance(handle));
    if (item == nullptr || item->chkDead() ||
        item->mAction != daDitem_c::ACTION_EVENT_e ||
        item_context.quantity(dItemNo_KAKERA_HEART_e) != 0) {
        return 4;
    }

    // A normal process frame before message acknowledgement must not acquire.
    if (!manager.execute_all() ||
        item_context.quantity(dItemNo_KAKERA_HEART_e) != 0 ||
        item_context.metrics.acquisitions != 0) {
        return 5;
    }

    // Message acknowledgement only marks the source actor dead.
    item->dead();
    if (!item->chkDead() ||
        item_context.quantity(dItemNo_KAKERA_HEART_e) != 0) {
        return 6;
    }

    // The following normal source process pass owns execItemGet().
    if (!manager.execute_all() ||
        item->mAction != daDitem_c::ACTION_WAIT_LIGHT_END_e ||
        item_context.quantity(dItemNo_KAKERA_HEART_e) != 1 ||
        item_context.metrics.acquisitions != 1 ||
        item_context.metrics.quantity_added != 1) {
        return 7;
    }

    // A second normal frame proves actionEvent()/execItemGet() cannot re-enter.
    if (!manager.execute_all() ||
        item_context.quantity(dItemNo_KAKERA_HEART_e) != 1 ||
        item_context.metrics.acquisitions != 1 ||
        item_context.metrics.quantity_added != 1 ||
        item_context.metrics.invalid_requests != 0 ||
        manager.metrics.errors != 0) {
        return 8;
    }

    if (!manager.destroy(handle) || manager.active_count() != 0) {
        return 9;
    }
    compat::unbind_original_tbox_context();
    process::unbind_process_manager();
    manager.shutdown();
    item_context.shutdown();
    interaction_context.shutdown();
    event_context.shutdown();

    std::puts(
        "DEMO_ITEM_COMMIT_HOST_OK item=0x21 source_owned=true "
        "commit_frames=2 acquisitions=1 duplicate_commits=0");
    return 0;
}
