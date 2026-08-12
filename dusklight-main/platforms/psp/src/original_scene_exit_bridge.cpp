#include "dusk/psp/original_scene_exit_bridge.hpp"

#include "d/actor/d_a_player.h"
#include "d/actor/d_a_scene_exit.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_mtx.h"

#include <cmath>
#include <cstring>

extern const actor_process_profile_definition2 g_profile_SCENE_EXIT;

const leafdraw_method_class g_fpcLf_Method = {};
actor_method_class g_fopAc_Method = {};
const u16 dSv_event_flag_c::saveBitLabels[822] = {};

namespace {

dusk::psp::compat::SceneExitFacade g_facade = {};
daPy_py_c g_player;
bool g_bound = false;
bool g_paused = false;
bool g_scene_change_started = false;
std::uint32_t g_request_calls = 0;
Mtx g_matrix = {};

}  // namespace

MtxP mDoMtx_stack_c::now = g_matrix;

BOOL dComIfGs_isSwitch(int number, int room) {
    return g_bound && g_facade.is_switch != nullptr &&
                   g_facade.is_switch(
                       g_facade.user, static_cast<s8>(room),
                       static_cast<u8>(number))
               ? TRUE : FALSE;
}

void dComIfGs_onSwitch(int number, int room) {
    if (g_bound && g_facade.on_switch != nullptr) {
        g_facade.on_switch(
            g_facade.user, static_cast<s8>(room),
            static_cast<u8>(number));
    }
}

void dComIfGs_offSwitch(int number, int room) {
    if (g_bound && g_facade.off_switch != nullptr) {
        g_facade.off_switch(
            g_facade.user, static_cast<s8>(room),
            static_cast<u8>(number));
    }
}

BOOL dComIfGs_isEventBit(u16 flag) {
    return g_bound && g_facade.is_event_bit != nullptr &&
                   g_facade.is_event_bit(g_facade.user, flag)
               ? TRUE : FALSE;
}

BOOL dComIfGs_isTbox(int number) {
    return g_bound && number >= 0 && number < 64 &&
                   g_facade.is_treasure_open != nullptr &&
                   g_facade.is_treasure_open(
                       g_facade.user, static_cast<u8>(number))
               ? TRUE : FALSE;
}

daPy_py_c* daPy_getPlayerActorClass() {
    return &g_player;
}

bool dusk_psp_compat_scene_change(
    u8 exit_index, u8 path_id, fopAc_ac_c* source, bool jump) {
    if (!g_bound || g_paused ||
        g_facade.request_transition == nullptr) {
        return false;
    }
    const float position[3] = {
        g_player.current.pos.x,
        g_player.current.pos.y,
        g_player.current.pos.z,
    };
    if (!g_facade.request_transition(
            g_facade.user, exit_index, path_id, jump, position)) {
        return false;
    }
    ++g_request_calls;
    g_scene_change_started = true;
    static_cast<daScex_c*>(source)->setSceneChangeOK();
    return true;
}

bool dusk_psp_compat_scene_change_started() {
    return g_scene_change_started;
}

void mDoMtx_stack_c::transS(f32 x, f32 y, f32 z) {
    std::memset(g_matrix, 0, sizeof(g_matrix));
    g_matrix[0][0] = 1.0f;
    g_matrix[1][1] = 1.0f;
    g_matrix[2][2] = 1.0f;
    g_matrix[0][3] = x;
    g_matrix[1][3] = y;
    g_matrix[2][3] = z;
}

void mDoMtx_stack_c::YrotM(s16 angle) {
    const f32 radians =
        static_cast<f32>(angle) *
        (6.28318530717958647692f / 65536.0f);
    const f32 cosine = std::cos(radians);
    const f32 sine = std::sin(radians);
    g_matrix[0][0] = cosine;
    g_matrix[0][2] = sine;
    g_matrix[2][0] = -sine;
    g_matrix[2][2] = cosine;
}

void mDoMtx_stack_c::ZXYrotM(s16 x, s16 y, s16 z) {
    constexpr f32 kAngleScale =
        6.28318530717958647692f / 65536.0f;
    const f32 sx = std::sin(static_cast<f32>(x) * kAngleScale);
    const f32 cx = std::cos(static_cast<f32>(x) * kAngleScale);
    const f32 sy = std::sin(static_cast<f32>(y) * kAngleScale);
    const f32 cy = std::cos(static_cast<f32>(y) * kAngleScale);
    const f32 sz = std::sin(static_cast<f32>(z) * kAngleScale);
    const f32 cz = std::cos(static_cast<f32>(z) * kAngleScale);
    const f32 translation[3] = {
        g_matrix[0][3], g_matrix[1][3], g_matrix[2][3]};
    g_matrix[0][0] = cz * cy - sz * sx * sy;
    g_matrix[0][1] = -sz * cx;
    g_matrix[0][2] = cz * sy + sz * sx * cy;
    g_matrix[1][0] = sz * cy + cz * sx * sy;
    g_matrix[1][1] = cz * cx;
    g_matrix[1][2] = sz * sy - cz * sx * cy;
    g_matrix[2][0] = -cx * sy;
    g_matrix[2][1] = sx;
    g_matrix[2][2] = cx * cy;
    g_matrix[0][3] = translation[0];
    g_matrix[1][3] = translation[1];
    g_matrix[2][3] = translation[2];
}

void mDoMtx_stack_c::transM(f32 x, f32 y, f32 z) {
    g_matrix[0][3] +=
        g_matrix[0][0] * x +
        g_matrix[0][1] * y +
        g_matrix[0][2] * z;
    g_matrix[1][3] +=
        g_matrix[1][0] * x +
        g_matrix[1][1] * y +
        g_matrix[1][2] * z;
    g_matrix[2][3] +=
        g_matrix[2][0] * x +
        g_matrix[2][1] * y +
        g_matrix[2][2] * z;
}

MtxP mDoMtx_stack_c::get() {
    return g_matrix;
}

void mDoMtx_inverse(MtxP source, Mtx destination) {
    const f32 cosine = source[0][0];
    const f32 sine = source[0][2];
    const f32 x = source[0][3];
    const f32 y = source[1][3];
    const f32 z = source[2][3];
    std::memset(destination, 0, sizeof(Mtx));
    destination[0][0] = cosine;
    destination[0][2] = -sine;
    destination[1][1] = 1.0f;
    destination[2][0] = sine;
    destination[2][2] = cosine;
    destination[0][3] = -cosine * x + sine * z;
    destination[1][3] = -y;
    destination[2][3] = -sine * x - cosine * z;
}

void mDoMtx_multVec(
    MtxP matrix, const cXyz* source, cXyz* destination) {
    destination->x =
        matrix[0][0] * source->x +
        matrix[0][1] * source->y +
        matrix[0][2] * source->z + matrix[0][3];
    destination->y =
        matrix[1][0] * source->x +
        matrix[1][1] * source->y +
        matrix[1][2] * source->z + matrix[1][3];
    destination->z =
        matrix[2][0] * source->x +
        matrix[2][1] * source->y +
        matrix[2][2] * source->z + matrix[2][3];
}

namespace dusk::psp::compat {

void bind_scene_exit_facade(const SceneExitFacade& facade) {
    g_facade = facade;
    g_bound = true;
    g_paused = false;
    g_scene_change_started = false;
    g_request_calls = 0;
    g_player.current.pos.set(0.0f, 0.0f, 0.0f);
}

void unbind_scene_exit_facade() {
    g_facade = {};
    g_bound = false;
    g_scene_change_started = false;
}

void set_scene_exit_player_position(const float position[3]) {
    if (position != nullptr) {
        g_player.current.pos.set(
            position[0], position[1], position[2]);
    }
}

void set_scene_exit_paused(bool paused) {
    g_paused = paused;
}

bool register_original_scene_exit_profile(
    process::PspProcessManager* manager) {
    return manager != nullptr &&
           manager->register_profile(&g_profile_SCENE_EXIT);
}

bool create_original_scene_exit(
    process::PspProcessManager* manager,
    const room::PackageView& scene,
    std::int8_t room_number,
    process::ProcessHandle* handle) {
    if (manager == nullptr || handle == nullptr || scene.bytes == nullptr ||
        room::read_u16(scene.bytes + 4) < 3) {
        return false;
    }
    const std::uint32_t count = room::read_u32(scene.bytes + 220);
    for (std::uint32_t index = 0; index < count; ++index) {
        room::SceneTriggerV3 trigger = {};
        if (room::read_dpsc_trigger_v3(
                scene, index, &trigger) != room::PackageError::Ok) {
            return false;
        }
        if (trigger.process_id != fpcNm_SCENE_EXIT_e) {
            continue;
        }
        const process::CreateInput input = {
            static_cast<std::int16_t>(trigger.process_id),
            trigger.parameters,
            {trigger.position[0], trigger.position[1], trigger.position[2]},
            {trigger.rotation[0], trigger.rotation[1], trigger.rotation[2]},
            {trigger.source_scale[0], trigger.source_scale[1],
             trigger.source_scale[2]},
            room_number,
            0x27F88D9Au,
            static_cast<std::uint16_t>(index),
            trigger.name_hash,
            0,
        };
        return manager->create(input, handle);
    }
    return false;
}

bool inspect_original_scene_exit(
    const process::PspProcessManager& manager,
    process::ProcessHandle handle,
    SceneExitSnapshot* snapshot) {
    if (snapshot == nullptr) {
        return false;
    }
    const auto* actor = static_cast<const daScex_c*>(
        manager.instance(handle));
    if (actor == nullptr) {
        return false;
    }
    snapshot->parameters = actor->parameters;
    snapshot->position[0] = actor->current.pos.x;
    snapshot->position[1] = actor->current.pos.y;
    snapshot->position[2] = actor->current.pos.z;
    snapshot->rotation[0] = actor->shape_angle.x;
    snapshot->rotation[1] = actor->shape_angle.y;
    snapshot->rotation[2] = actor->shape_angle.z;
    snapshot->dimensions[0] = actor->scale.x;
    snapshot->dimensions[1] = actor->scale.y;
    snapshot->dimensions[2] = actor->scale.z;
    snapshot->room = actor->home.roomNo;
    return true;
}

std::uint32_t original_transition_request_calls() {
    return g_request_calls;
}

std::uint32_t specialized_psp_trigger_logic_calls() {
    return 0;
}

bool original_profile_valid() {
    return g_profile_SCENE_EXIT.base.base.base.name ==
               fpcNm_SCENE_EXIT_e &&
           g_profile_SCENE_EXIT.base.base.base.process_size ==
               sizeof(daScex_c) &&
           g_profile_SCENE_EXIT.base.sub_method != nullptr;
}

}  // namespace dusk::psp::compat
