#include "dusk/psp/original_tbox_bridge.hpp"

#include "d/actor/d_a_tbox.h"
#include "d/actor/d_a_demo_item.h"
#include "d/d_item.h"
#include "d/actor/d_a_player.h"
#include "dusk/psp/process_runtime.hpp"
#include "dusk/psp/source_event_script.hpp"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_mtx.h"

#include <cmath>
#include <cstring>

extern const actor_process_profile_definition g_profile_TBOX;
extern const actor_process_profile_definition g_profile_Demo_Item;

namespace {

using dusk::psp::compat::OriginalTboxMetrics;

dusk::psp::process::PspProcessManager* g_manager = nullptr;
dusk::psp::events::PspEventContext* g_events = nullptr;
dusk::psp::interaction::PspInteractionContext* g_interactions = nullptr;
dusk::psp::items::PspItemContext* g_items = nullptr;
OriginalTboxMetrics g_metrics = {};
daTbox_c* g_tboxes[2] = {};
std::uint16_t g_tbox_count = 0;
fpc_ProcID g_next_item_id = 0x70000000u;
struct PendingDemoItem {
    fpc_ProcID id = fpcM_ERROR_PROCESS_ID_e;
    std::uint8_t item_no = dItemNo_NONE_e;
    const void* source_actor = nullptr;
    bool active = false;
    bool visible = false;
    bool dead = false;
    bool committed = false;
};
PendingDemoItem g_pending_demo_items[4] = {};

dEvent_manager_c g_event_manager;
camera_process_class g_camera;
dCcS_CompatWorld g_collision_world;
daMidna_c g_midna;
u8 g_event_registers[256] = {};
cXyz g_line_cross = {};

daTbox_c* current_tbox() {
    return static_cast<daTbox_c*>(
        dusk::psp::process::current_instance());
}

void multiply(MtxP left, MtxP right, Mtx destination) {
    Mtx result = {};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            for (int inner = 0; inner < 3; ++inner) {
                result[row][column] +=
                    left[row][inner] * right[inner][column];
            }
        }
        result[row][3] = left[row][3];
        for (int inner = 0; inner < 3; ++inner) {
            result[row][3] += left[row][inner] * right[inner][3];
        }
    }
    MTXCopy(result, destination);
}

PendingDemoItem* pending_item(fpc_ProcID id) {
    for (auto& item : g_pending_demo_items) {
        if (item.active && item.id == id) return &item;
    }
    return nullptr;
}

const PendingDemoItem* pending_item_const(fpc_ProcID id) {
    for (const auto& item : g_pending_demo_items) {
        if (item.active && item.id == id) return &item;
    }
    return nullptr;
}

void reset_pending_items() {
    for (auto& item : g_pending_demo_items) item = {};
}

fpc_ProcID create_item(u8 item) {
    daTbox_c* actor = current_tbox();
    if (actor == nullptr || g_items == nullptr) {
        return fpcM_ERROR_PROCESS_ID_e;
    }
    auto* script = dusk::psp::events::source_event_script();
    if (script != nullptr && script->running()) {
        for (auto& pending : g_pending_demo_items) {
            if (pending.active) continue;
            pending.id = g_next_item_id++;
            pending.item_no = item;
            pending.source_actor = actor;
            pending.active = true;
            ++g_metrics.items_created;
            return pending.id;
        }
        return fpcM_ERROR_PROCESS_ID_e;
    }
    // Compatibility fallback for isolated legacy host tests that do not bind
    // an event_list.dat runtime yet.
    if (!g_items->acquire(item, 1, actor)) {
        return fpcM_ERROR_PROCESS_ID_e;
    }
    ++g_metrics.items_created;
    ++g_metrics.items_committed;
    return g_next_item_id++;
}

bool event_completed() {
    return g_events != nullptr &&
           g_events->state() == dusk::psp::events::State::Completed;
}

}  // namespace

const cXyz cXyz::BaseX = {1.0f, 0.0f, 0.0f};
const cXyz cXyz::BaseY = {0.0f, 1.0f, 0.0f};

void mDoMtx_identity(Mtx destination) {
    std::memset(destination, 0, sizeof(Mtx));
    destination[0][0] = 1.0f;
    destination[1][1] = 1.0f;
    destination[2][2] = 1.0f;
}

void mDoMtx_copy(MtxP source, Mtx destination) {
    MTXCopy(source, destination);
}

void mDoMtx_concat(MtxP left, MtxP right, Mtx destination) {
    multiply(left, right, destination);
}

void MTXRotAxisRad(
    Mtx destination, const cXyz* axis, f32 radians) {
    mDoMtx_identity(destination);
    if (axis == nullptr) {
        return;
    }
    const f32 length = std::sqrt(
        axis->x * axis->x + axis->y * axis->y + axis->z * axis->z);
    if (length <= 0.00001f) {
        return;
    }
    const f32 x = axis->x / length;
    const f32 y = axis->y / length;
    const f32 z = axis->z / length;
    const f32 cosine = std::cos(radians);
    const f32 sine = std::sin(radians);
    const f32 one_minus = 1.0f - cosine;
    destination[0][0] = cosine + x * x * one_minus;
    destination[0][1] = x * y * one_minus - z * sine;
    destination[0][2] = x * z * one_minus + y * sine;
    destination[1][0] = y * x * one_minus + z * sine;
    destination[1][1] = cosine + y * y * one_minus;
    destination[1][2] = y * z * one_minus - x * sine;
    destination[2][0] = z * x * one_minus - y * sine;
    destination[2][1] = z * y * one_minus + x * sine;
    destination[2][2] = cosine + z * z * one_minus;
}

void mDoMtx_stack_c::YrotS(s16 angle) {
    mDoMtx_identity(now);
    YrotM(angle);
}

void mDoMtx_stack_c::concat(MtxP matrix) {
    Mtx result = {};
    multiply(now, matrix, result);
    MTXCopy(result, now);
}

void mDoMtx_stack_c::multVec(
    const cXyz* source, cXyz* destination) {
    mDoMtx_multVec(now, source, destination);
}

void cLib_offsetPos(
    cXyz* destination, const cXyz* origin,
    s16 angle, const cXyz* offset) {
    if (destination == nullptr || origin == nullptr || offset == nullptr) {
        return;
    }
    const f32 radians =
        static_cast<f32>(angle) * 3.14159265358979323846f / 32768.0f;
    const f32 sine = std::sin(radians);
    const f32 cosine = std::cos(radians);
    destination->x = origin->x + offset->x * cosine + offset->z * sine;
    destination->y = origin->y + offset->y;
    destination->z = origin->z - offset->x * sine + offset->z * cosine;
}

void cLib_addCalc0(f32* value, f32 scale, f32 maximum) {
    if (value == nullptr) {
        return;
    }
    const f32 amount = std::fmin(std::fabs(*value) * scale, maximum);
    if (*value > amount) {
        *value -= amount;
    } else if (*value < -amount) {
        *value += amount;
    } else {
        *value = 0.0f;
    }
}

void cLib_addCalcAngleS(
    s16* value, s16 target, int scale, s16 maximum, s16 minimum) {
    if (value == nullptr || scale <= 0) {
        return;
    }
    int delta = static_cast<s16>(target - *value);
    int step = delta / scale;
    if (step > maximum) step = maximum;
    if (step < -maximum) step = -maximum;
    if (step != 0 && std::abs(step) < minimum) {
        step = step < 0 ? -minimum : minimum;
    }
    *value = static_cast<s16>(*value + step);
}

dCcS_CompatWorld* dComIfG_Ccsp() {
    return &g_collision_world;
}

void dKy_efplight_cut(LIGHT_INFLUENCE*) {}
void dKy_efplight_set(LIGHT_INFLUENCE*) {}
void dKy_set_allcol_ratio(f32) {}

dPath* dPath_GetRoomPath(int, int) {
    return nullptr;
}

void dTres_c::onStatus(int, int, int) {}
void dTres_c::offStatus(int, int, int) {}
void dTres_c::setPosition(int, const cXyz*) {}

bool dItem_data::chkFlag(u8, u32) {
    return false;
}

daMidna_c* daPy_py_c::getMidnaActor() {
    return &g_midna;
}

camera_process_class* dComIfGp_getCamera(int) {
    return &g_camera;
}

int dComIfGp_getPlayerCameraID(int) {
    return 0;
}

fopAc_ac_c* dComIfGp_getPlayer(int) {
    return daPy_getPlayerActorClass();
}

dStage_roomDt_c* dComIfGp_roomControl_getStatusRoomDt(int) {
    return nullptr;
}

const char* dComIfGp_getStartStageName() {
    return "D_MN10";
}

int dComIfGp_roomControl_getStayNo() {
    if (auto* actor = static_cast<fopAc_ac_c*>(dusk::psp::process::current_instance())) {
        return actor->current.roomNo;
    }
    return 2;
}

BOOL dComIfGs_isTmpBit(u16) { return FALSE; }
int dComIfG_play_c::getLayerNo(int) { return 0; }

dStage_MapEvent_dt_c* dEvt_control_c::searchMapEventData(u8, s32) {
    return nullptr;
}

s16 dEvent_manager_c::getEventIdx(fopAc_ac_c* actor, u8 event_id) {
    const s16 result = event_id == 0xFF ? static_cast<s16>(1)
                                       : static_cast<s16>(event_id);
    if (actor != nullptr) actor->eventInfo.setEventId(result);
    return result;
}

s16 dEvent_manager_c::getEventIdx(
    fopAc_ac_c* actor, const char* event_name, u8) {
    if (auto* script = dusk::psp::events::source_event_script();
        script != nullptr && script->initialized() && event_name != nullptr) {
        const s16 id = script->event_id(event_name);
        if (actor != nullptr) actor->eventInfo.setEventId(id);
        return id;
    }
    if (actor != nullptr) actor->eventInfo.setEventId(1);
    return 1;
}

int dEvent_manager_c::getMyStaffId(
    const char* name, fopAc_ac_c*, int tag) {
    if (auto* script = dusk::psp::events::source_event_script();
        script != nullptr && script->running()) {
        return script->staff_id(name, tag);
    }
    return 1;
}

dEvent_manager_c& dComIfGp_getEventManager() {
    return g_event_manager;
}

void dComIfGp_event_onEventFlag(u16) {}

void dComIfGp_event_reset() {
    if (daTbox_c* actor = current_tbox()) {
        actor->eventInfo.setCommand(0);
        actor->eventInfo.clearCondition();
    }
    if (g_events != nullptr &&
        g_events->state() != dusk::psp::events::State::None) {
        g_events->reset();
    }
    if (g_interactions != nullptr) {
        g_interactions->complete();
    }
    if (auto* script = dusk::psp::events::source_event_script();
        script != nullptr && script->initialized()) {
        script->reset();
    }
}

void dComIfGp_event_setItemPartner(void*) {}

void dComIfGp_event_setItemPartnerId(fpc_ProcID id) {
    if (g_events != nullptr) {
        g_events->set_item_partner(id);
    }
}

int dComIfGp_evmng_getMyStaffId(
    const char* name, fopAc_ac_c*, int tag) {
    if (auto* script = dusk::psp::events::source_event_script();
        script != nullptr && script->running()) {
        return script->staff_id(name, tag);
    }
    return 1;
}

int dComIfGp_evmng_getMyActIdx(
    int staff, const char* const* actions, int count, int, int prefix) {
    if (auto* script = dusk::psp::events::source_event_script();
        script != nullptr && script->running()) {
        return script->action_index(staff, actions, count, prefix != 0);
    }
    return 1;
}

int dComIfGp_evmng_getIsAddvance(int staff) {
    if (auto* script = dusk::psp::events::source_event_script();
        script != nullptr && script->running()) {
        return script->staff_advanced(staff) ? 1 : 0;
    }
    return 1;
}

void dComIfGp_evmng_cutEnd(int staff) {
    if (auto* script = dusk::psp::events::source_event_script();
        script != nullptr && script->running()) {
        script->cut_end(staff);
        return;
    }
    daTbox_c* actor = current_tbox();
    if (g_events != nullptr && actor != nullptr &&
        g_events->state() == dusk::psp::events::State::Running &&
        g_events->complete(actor)) {
        ++g_metrics.events_completed;
    }
}

int dComIfGp_evmng_endCheck(s16 event_id) {
    if (auto* script = dusk::psp::events::source_event_script();
        script != nullptr && script->initialized()) {
        const bool ended = script->end_check(event_id);
        if (ended && g_events != nullptr &&
            g_events->state() == dusk::psp::events::State::Running) {
            if (daTbox_c* actor = current_tbox();
                actor != nullptr && g_events->complete(actor)) {
                ++g_metrics.events_completed;
            }
        }
        return ended ? 1 : 0;
    }
    return event_completed() ? 1 : 0;
}

int dComIfGp_evmng_endCheck(const char* event_name) {
    if (auto* script = dusk::psp::events::source_event_script();
        script != nullptr && script->initialized()) {
        return script->end_check(event_name) ? 1 : 0;
    }
    return event_completed() ? 1 : 0;
}

u8 dComIfGs_getItem(int, bool) {
    return dItemNo_NONE_e;
}

u8 dComIfGs_getBombMax(u8) {
    return 0;
}

u8 dComIfGs_getBombNum(int) {
    return 0;
}

u8 dComIfGs_getEventReg(u16 id) {
    return g_event_registers[id & 0xFFu];
}

void dComIfGs_setEventReg(u16 id, u8 value) {
    g_event_registers[id & 0xFFu] = value;
}

void dComIfGs_onTbox(int number) {
    if (g_items != nullptr && g_items->set_treasure_open(number)) {
        ++g_metrics.collision_swaps;
    }
}

fpc_ProcID fopAcM_createItemForTrBoxDemo(
    const cXyz* position, u8 item, int item_bit, int room,
    const void* angle_ptr, const void* scale_ptr) {
    if (g_manager != nullptr &&
        g_manager->profile_registered(fpcNm_Demo_Item_e)) {
        const auto* angle = static_cast<const csXyz*>(angle_ptr);
        const auto* wanted_scale = static_cast<const cXyz*>(scale_ptr);
        dusk::psp::process::CreateInput input = {};
        input.process_id = fpcNm_Demo_Item_e;
        input.parameters = static_cast<u32>(item) |
            ((static_cast<u32>(item_bit) & 0x7Fu) << 8);
        input.position[0] = position != nullptr ? position->x : 0.0f;
        input.position[1] = position != nullptr ? position->y : 0.0f;
        input.position[2] = position != nullptr ? position->z : 0.0f;
        input.rotation[0] = angle != nullptr ? angle->x : 0;
        input.rotation[1] = angle != nullptr ? angle->y : 0;
        input.rotation[2] = angle != nullptr ? angle->z : 0;
        input.scale[0] = wanted_scale != nullptr ? wanted_scale->x : 1.0f;
        input.scale[1] = wanted_scale != nullptr ? wanted_scale->y : 1.0f;
        input.scale[2] = wanted_scale != nullptr ? wanted_scale->z : 1.0f;
        input.room = static_cast<s8>(room);
        input.layer = -1;
        dusk::psp::process::ProcessHandle handle = {};
        if (!g_manager->create(input, &handle)) return fpcM_ERROR_PROCESS_ID_e;
        void* actor = g_manager->instance(handle);
        if (actor == nullptr) return fpcM_ERROR_PROCESS_ID_e;
        ++g_metrics.items_created;
        return g_manager->id_of(actor);
    }
    return create_item(item);
}

fpc_ProcID fopAcM_createItemForPresentDemo(
    const cXyz*, u8 item, int, int, int, const void*, const void*) {
    return create_item(item);
}

int fopAcM_orderOtherEvent(
    fopAc_ac_c*, const char*, u16, u16, u8) {
    return 1;
}

int fopAcM_orderOtherEventId(
    fopAc_ac_c*, s16, u8, u16, u8, u8) {
    return 1;
}

int fopAcM_seenPlayerAngleY(const fopAc_ac_c*) {
    return 0;
}

int fopAcM_seenActorAngleY(
    const fopAc_ac_c*, const fopAc_ac_c*) {
    return 0;
}

bool fopAcM_myRoomSearchEnemy(int) {
    return false;
}

void fopAcM_posMoveF(fopAc_ac_c* actor, const cXyz*) {
    if (actor == nullptr) {
        return;
    }
    actor->old.pos = actor->current.pos;
    actor->speed.y += actor->gravity;
    actor->current.pos.x += actor->speed.x;
    actor->current.pos.y += actor->speed.y;
    actor->current.pos.z += actor->speed.z;
}

bool fopAcM_lc_c::lineCheck(
    const cXyz*, const cXyz* end, const fopAc_ac_c*) {
    if (end != nullptr) {
        g_line_cross = *end;
    }
    return true;
}

bool fopAcM_lc_c::checkMoveBG() {
    return false;
}

cXyz* fopAcM_lc_c::getCrossP() {
    return &g_line_cross;
}

bool fopAcM_gc_c::gndCheck(const cXyz* position) {
    if (position != nullptr) {
        g_line_cross = *position;
    }
    return true;
}

f32 fopAcM_gc_c::getGroundY() {
    return g_line_cross.y;
}

namespace dusk::psp::compat {

bool register_original_tbox_profile(
    process::PspProcessManager* manager) {
    return manager != nullptr &&
           manager->register_profile(&g_profile_TBOX) &&
           manager->register_profile(&g_profile_Demo_Item);
}

bool original_tbox_profile_valid() {
    return g_profile_TBOX.base.base.name == fpcNm_TBOX_e &&
           g_profile_TBOX.base.base.process_size == sizeof(daTbox_c) &&
           g_profile_TBOX.sub_method != nullptr &&
           sizeof(daTbox_c) <= process::kInstanceStorage;
}

bool bind_original_tbox_context(
    process::PspProcessManager* manager,
    events::PspEventContext* events,
    interaction::PspInteractionContext* interactions,
    items::PspItemContext* items) {
    if (manager == nullptr || events == nullptr ||
        interactions == nullptr || items == nullptr ||
        !events->initialized() || !interactions->initialized() ||
        !items->initialized()) {
        return false;
    }
    g_manager = manager;
    g_events = events;
    g_interactions = interactions;
    g_items = items;
    g_tbox_count = 0;
    std::memset(g_tboxes, 0, sizeof(g_tboxes));
    g_metrics = {};
    reset_pending_items();
    g_metrics.source_profile_valid = original_tbox_profile_valid();
    return g_metrics.source_profile_valid;
}

void unbind_original_tbox_context() {
    g_manager = nullptr;
    g_events = nullptr;
    g_interactions = nullptr;
    g_items = nullptr;
    g_tbox_count = 0;
    std::memset(g_tboxes, 0, sizeof(g_tboxes));
    reset_pending_items();
}

bool create_original_tboxes(
    process::PspProcessManager* manager,
    const room::PackageView& scene,
    std::int8_t room_number,
    process::ProcessHandle* handles,
    std::uint16_t handle_capacity,
    std::uint16_t* created) {
    if (manager == nullptr || manager != g_manager ||
        scene.bytes == nullptr || handles == nullptr ||
        created == nullptr) {
        return false;
    }
    *created = 0;
    if (room_number != 2) {
        return true;
    }
    if (g_tbox_count != 0) {
        return false;
    }
    bool parameters_preserved = true;
    const std::uint32_t count = room::read_u32(scene.bytes + 136);
    for (std::uint32_t index = 0; index < count; ++index) {
        room::SceneActorV3 placement = {};
        if (room::read_dpsc_actor_v3(scene, index, &placement) !=
            room::PackageError::Ok) {
            return false;
        }
        if (placement.name_hash != 0x2A0E83C6u) {
            continue;
        }
        ++g_metrics.placements_seen;
        const std::uint32_t shape = (placement.parameters >> 20) & 0xFu;
        const std::uint32_t function = placement.parameters & 0x3Fu;
        if (shape == 2) {
            ++g_metrics.unsupported_boss_placements;
            continue;
        }
        if (placement.process_id != fpcNm_TBOX_e ||
            shape != 1 || function != 0 ||
            placement.room != static_cast<std::uint8_t>(room_number) ||
            *created >= handle_capacity || g_tbox_count >= 2) {
            return false;
        }
        const process::CreateInput input = {
            static_cast<std::int16_t>(placement.process_id),
            placement.parameters,
            {placement.position[0], placement.position[1],
             placement.position[2]},
            {placement.rotation[0], placement.rotation[1],
             placement.rotation[2]},
            {placement.scale[0], placement.scale[1], placement.scale[2]},
            room_number,
            placement.table_hash,
            placement.source_index,
            placement.name_hash,
            static_cast<std::int8_t>(placement.layer),
        };
        if (!manager->create(input, &handles[*created])) {
            return false;
        }
        auto* actor = static_cast<daTbox_c*>(
            manager->instance(handles[*created]));
        const std::uint32_t expected_tbox =
            (placement.parameters >> 6) & 0x3Fu;
        const std::uint32_t expected_item =
            (static_cast<std::uint16_t>(placement.rotation[2]) >> 8) &
            0xFFu;
        parameters_preserved =
            parameters_preserved && actor != nullptr &&
            static_cast<std::uint32_t>(actor->getTboxNo()) ==
                expected_tbox &&
            static_cast<std::uint32_t>(actor->getItemNo()) ==
                expected_item;
        g_tboxes[g_tbox_count++] = actor;
        ++*created;
        ++g_metrics.placements_created;
    }
    g_metrics.parameters_preserved =
        parameters_preserved && *created == 2;
    return g_metrics.parameters_preserved;
}

void deactivate_original_tboxes(std::int8_t room_number) {
    if (room_number != 2) {
        return;
    }
    g_tbox_count = 0;
    std::memset(g_tboxes, 0, sizeof(g_tboxes));
}

void set_original_tbox_player_position(const float position[3]) {
    if (position == nullptr) {
        return;
    }
    daPy_py_c* player = daPy_getPlayerActorClass();
    player->current.pos.set(position[0], position[1], position[2]);
    player->attention_info.position = player->current.pos;
}

bool set_original_tbox_validation_player_position() {
    for (std::uint16_t index = 0; index < g_tbox_count; ++index) {
        if (g_tboxes[index] != nullptr &&
            g_tboxes[index]->getItemNo() == dItemNo_KAKERA_HEART_e) {
            const float position[3] = {
                g_tboxes[index]->current.pos.x,
                g_tboxes[index]->current.pos.y,
                g_tboxes[index]->current.pos.z,
            };
            set_original_tbox_player_position(position);
            return true;
        }
    }
    return g_tbox_count == 0;
}

bool sample_original_tbox_interaction(
    process::PspProcessManager* manager,
    bool press_open) {
    if (manager == nullptr || manager != g_manager ||
        g_interactions == nullptr || g_events == nullptr) {
        return false;
    }
    daTbox_c* candidate = nullptr;
    for (std::uint16_t index = 0; index < g_tbox_count; ++index) {
        if (g_tboxes[index] != nullptr &&
            g_tboxes[index]->getItemNo() == dItemNo_KAKERA_HEART_e) {
            candidate = g_tboxes[index];
            break;
        }
    }
    if (candidate == nullptr || (candidate->eventInfo.condition() & 4u) == 0) {
        return true;
    }
    g_interactions->begin_frame();
    const interaction::Candidate interaction_candidate = {
        candidate,
        "OPEN",
        interaction::ActionType::Open,
        interaction::Button::Cross,
        0.0f,
        0,
        10,
        false,
    };
    if (!g_interactions->publish(interaction_candidate)) {
        return false;
    }
    ++g_metrics.interaction_requests;
    if (!press_open) {
        return true;
    }
    s16 event_id = candidate->eventInfo.getEventId();
    auto* script = dusk::psp::events::source_event_script();
    if (script != nullptr && script->initialized() &&
        candidate->eventInfo.eventName() != nullptr) {
        event_id = script->event_id(candidate->eventInfo.eventName());
        candidate->eventInfo.setEventId(event_id);
    }
    const events::Request request = {
        candidate, event_id, 0, events::Kind::Item,
    };
    if (!g_interactions->accept(interaction::Button::Cross) ||
        !g_events->request(request) || !g_events->accept(candidate) ||
        !g_events->start(candidate) ||
        (script != nullptr && script->initialized() &&
         !script->start(event_id, candidate))) {
        return false;
    }
    candidate->eventInfo.setCommand(3);
    candidate->eventInfo.clearCondition();
    ++g_metrics.interactions_accepted;
    return true;
}

bool original_tbox_demo_item_pending(
    std::uint32_t process_id, std::uint8_t* item_no) {
    if (g_manager != nullptr) {
        void* raw = g_manager->search_by_id(process_id);
        if (raw != nullptr && g_manager->process_id_of(raw) == fpcNm_Demo_Item_e) {
            auto* actor = static_cast<daDitem_c*>(raw);
            if (item_no != nullptr) *item_no = actor->getItemNo();
            return !actor->chkDead();
        }
    }
    const PendingDemoItem* item = pending_item_const(process_id);
    if (item == nullptr) return false;
    if (item_no != nullptr) *item_no = item->item_no;
    return true;
}

bool original_tbox_demo_item_visible(std::uint32_t process_id) {
    if (g_manager != nullptr) {
        void* raw = g_manager->search_by_id(process_id);
        if (raw != nullptr && g_manager->process_id_of(raw) == fpcNm_Demo_Item_e) {
            auto* actor = static_cast<daDitem_c*>(raw);
            return actor->chkDraw() && !actor->chkDead();
        }
    }
    const PendingDemoItem* item = pending_item_const(process_id);
    return item != nullptr && item->visible && !item->dead;
}

bool original_tbox_demo_item_show(std::uint32_t process_id) {
    if (g_manager != nullptr) {
        void* raw = g_manager->search_by_id(process_id);
        if (raw != nullptr && g_manager->process_id_of(raw) == fpcNm_Demo_Item_e) {
            auto* actor = static_cast<daDitem_c*>(raw);
            if (actor->chkDead()) return false;
            if (!actor->chkDraw()) { actor->show(); ++g_metrics.items_shown; }
            return true;
        }
    }
    PendingDemoItem* item = pending_item(process_id);
    if (item == nullptr || item->dead) return false;
    if (!item->visible) { item->visible = true; ++g_metrics.items_shown; }
    return true;
}

bool original_tbox_demo_item_kill_and_commit(std::uint32_t process_id) {
    if (g_manager != nullptr) {
        void* raw = g_manager->search_by_id(process_id);
        if (raw != nullptr && g_manager->process_id_of(raw) == fpcNm_Demo_Item_e) {
            auto* actor = static_cast<daDitem_c*>(raw);
            if (actor->chkDead() || g_items == nullptr) return false;
            const std::uint32_t before = g_items->quantity(actor->getItemNo());
            actor->dead();
            ++g_metrics.items_killed;
            actor->actionEvent();
            const std::uint32_t after = g_items->quantity(actor->getItemNo());
            if (after != before + 1u) return false;
            ++g_metrics.items_committed;
            return true;
        }
    }
    PendingDemoItem* item = pending_item(process_id);
    if (item == nullptr || item->dead || item->committed ||
        g_items == nullptr || item->source_actor == nullptr) return false;
    item->dead = true; ++g_metrics.items_killed;
    if (!g_items->acquire(item->item_no, 1, item->source_actor)) return false;
    item->committed = true; ++g_metrics.items_committed;
    return true;
}

const OriginalTboxMetrics& original_tbox_metrics() {
    return g_metrics;
}

}  // namespace dusk::psp::compat


void execItemGet(u8 item_no) {
    if (g_items != nullptr) {
        g_items->acquire(item_no, 1, dusk::psp::process::current_instance());
    }
}
