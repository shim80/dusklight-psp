#include "d/actor/d_a_player.h"
#include "d/actor/d_a_tbox.h"
#include "dusk/psp/event_runtime.hpp"
#include "dusk/psp/interaction_runtime.hpp"
#include "dusk/psp/item_runtime.hpp"
#include "dusk/psp/link_fidelity.hpp"
#include "dusk/psp/model_runtime.hpp"
#include "dusk/psp/movebg_runtime.hpp"
#include "dusk/psp/original_scene_exit_bridge.hpp"
#include "dusk/psp/original_tbox_bridge.hpp"
#include "dusk/psp/platform.hpp"
#include "dusk/psp/playable_package.hpp"
#include "dusk/psp/playable_render.hpp"
#include "dusk/psp/playable_runtime.hpp"
#include "dusk/psp/presentation_profile.hpp"
#include "dusk/psp/process_runtime.hpp"
#include "dusk/psp/render_queue.hpp"
#include "dusk/psp/resource_manager.hpp"
#include "dusk/psp/room_package.hpp"
#include "dusk/psp/source_event_script.hpp"
#include "dusk/psp/source_getitem_camera.hpp"
#include "dusk/psp/source_message_bmg.hpp"

#include <pspkernel.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

PSP_MODULE_INFO("DuskSourceTreasure", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-256);

namespace compat = dusk::psp::compat;
namespace events = dusk::psp::events;
namespace interaction = dusk::psp::interaction;
namespace items = dusk::psp::items;
namespace link = dusk::psp::link;
namespace model = dusk::psp::model;
namespace movebg = dusk::psp::movebg;
namespace p = dusk::psp::playable;
namespace process = dusk::psp::process;
namespace render = dusk::psp::render;
namespace resources = dusk::psp::resources;
namespace room = dusk::psp::room;
namespace camera = dusk::psp::camera;
namespace message = dusk::psp::message;

namespace {
constexpr char kDir[] = "ms0:/PSP/GAME/DUSKLIGHT_CHEST_SOURCE_FULL";
constexpr std::uint32_t kCommandBytes = 512u * 1024u;
constexpr std::uint32_t kTboxHash = 0x2A0E83C6u;
constexpr std::uint8_t kHeart = dItemNo_KAKERA_HEART_e;
constexpr std::uint32_t kBoxop = 0x55u;
constexpr std::uint32_t kGetA = 0x169u;
constexpr std::uint32_t kGetAWait = 0x16Au;
constexpr float kTau = 6.28318530717958647692f;

alignas(16) std::uint8_t g_manifest[12000];
alignas(16) std::uint8_t g_link_model[300000];
alignas(16) std::uint8_t g_link_textures[460000];
alignas(16) std::uint8_t g_link_animation[460000];
alignas(16) std::uint8_t g_ui[180000];
alignas(16) std::uint8_t g_room_model[720000];
alignas(16) std::uint8_t g_room_textures[540000];
alignas(16) std::uint8_t g_room_scene[18000];
alignas(16) std::uint8_t g_event_list[120000];
alignas(16) std::uint8_t g_message_bmg[330000];
alignas(16) std::uint8_t g_heart_model[40000];
alignas(16) std::uint8_t g_heart_textures[70000];
alignas(16) std::uint8_t g_command[kCommandBytes];

resources::PspResourceManager g_resources;
render::PspRenderQueue g_queue;
movebg::PspMoveBgWorld g_world;
model::PspStaticModelRuntime g_models;
process::PspProcessManager g_processes;
events::PspEventContext g_event_context;
interaction::PspInteractionContext g_interactions;
items::PspItemContext g_items;
events::SourceEventScript g_script;
message::SourceBmgDatabase g_message_database;
message::SourceMessageRuntime g_message_runtime;

struct ViewState {
    room::Vec3 center = {1300.0f, 180.0f, -2800.0f};
    room::Vec3 eye = {1300.0f, 220.0f, -2600.0f};
    float fov = 52.0f;
};

struct ModelCollector {
    p::StaticModelRenderView views[4] = {};
    std::uint16_t count = 0;
};

bool path(const char* leaf, char out[256]) {
    return dusk::psp::make_game_path(leaf, out, 256);
}
bool read_asset(const char* leaf, void* out, std::uint32_t cap, std::uint32_t* size) {
    char f[256] = {};
    return path(leaf, f) && dusk::psp::read_file(f, out, cap, size);
}
bool write_marker(const char* leaf, const char* text) {
    char f[256] = {};
    return path(leaf, f) && dusk::psp::write_file(
        f, text, static_cast<std::uint32_t>(std::strlen(text)));
}
int fail(int code) {
    char text[64] = {};
    std::snprintf(text, sizeof(text), "code=%d\n", code);
    write_marker("CHEST_SOURCE_FULL.FAIL", text);
    dusk::psp::sleep_microseconds(1800000);
    p::shutdown_renderer();
    dusk::psp::shutdown();
    sceKernelExitGame();
    return code;
}

bool resource_reader(
    void*, const char* requested, void* output,
    std::uint32_t capacity, std::uint32_t* size) {
    if (requested == nullptr || output == nullptr || size == nullptr) return false;
    std::FILE* file = std::fopen(requested, "rb");
    if (file == nullptr) return false;
    if (std::fseek(file, 0, SEEK_END) != 0) { std::fclose(file); return false; }
    const long end = std::ftell(file);
    if (end <= 0 || static_cast<std::uint32_t>(end) > capacity ||
        std::fseek(file, 0, SEEK_SET) != 0) { std::fclose(file); return false; }
    const std::size_t got = std::fread(output, 1, static_cast<std::size_t>(end), file);
    std::fclose(file);
    if (got != static_cast<std::size_t>(end)) return false;
    *size = static_cast<std::uint32_t>(end);
    return true;
}

bool treasure_query(void* user, std::uint8_t number) {
    return static_cast<items::PspItemContext*>(user)->is_treasure_open(number);
}

bool collect_model(void* user, const render::Command& command) {
    auto* collector = static_cast<ModelCollector*>(user);
    if (collector == nullptr || command.kind != model::kStaticModelCommand ||
        command.payload == nullptr || collector->count >= 4) return false;
    const auto* source = static_cast<const J3DModel*>(command.payload);
    const J3DModelData* data = source->getModelData();
    if (!source->active() || data == nullptr || data->model_bytes() == nullptr ||
        data->texture_bytes() == nullptr) return false;
    auto& view = collector->views[collector->count++];
    view.model = data->model_bytes();
    view.model_size = data->model_size();
    view.textures = data->texture_bytes();
    view.texture_size = data->texture_size();
    std::memcpy(view.matrix, source->getBaseTRMtx(), sizeof(view.matrix));
    const cXyz& scale = source->getBaseScale();
    view.scale[0] = scale.x; view.scale[1] = scale.y; view.scale[2] = scale.z;
    return true;
}

void identity_at(float matrix[12], float x, float y, float z) {
    std::memset(matrix, 0, sizeof(float) * 12);
    matrix[0] = matrix[5] = matrix[10] = 1.0f;
    matrix[3] = x; matrix[7] = y; matrix[11] = z;
}

void rotate_y(float x, float z, float yaw, float* out_x, float* out_z) {
    const float s = std::sin(yaw), c = std::cos(yaw);
    *out_x = x * c + z * s;
    *out_z = z * c - x * s;
}

std::int16_t source_geta_yaw(
    std::int16_t initial, float frame) {
    if (frame < 9.0f) return initial;
    if (frame >= 16.0f) return static_cast<std::int16_t>(initial + 0x8000u);
    const float delta = (-32768.0f) * (frame - 9.0f) * (1.0f / 7.0f);
    return static_cast<std::int16_t>(
        static_cast<float>(initial) - delta);
}

room::Vec3 relative_to_actor(
    const room::Vec3& origin, std::int16_t yaw_s16,
    const events::SourceEventVec3& local) {
    const float yaw = link::s16_to_radians(static_cast<std::uint16_t>(yaw_s16));
    float x = 0.0f, z = 0.0f;
    rotate_y(local.x, local.z, yaw, &x, &z);
    return {origin.x + x, origin.y + local.y, origin.z + z};
}

p::RealRoomRenderInput render_input(
    const p::Runtime& runtime, const room::Vec3& link_pos,
    float link_yaw, const ViewState& camera_state) {
    p::RealRoomRenderInput in = {};
    in.link_position = {link_pos.x, link_pos.y, link_pos.z};
    in.link_yaw = link_yaw;
    in.camera_center = {camera_state.center.x, camera_state.center.y, camera_state.center.z};
    in.camera_eye = {camera_state.eye.x, camera_state.eye.y, camera_state.eye.z};
    in.camera_fov = camera_state.fov;
    in.presentation = dusk::psp::presentation::Profile::Game;
    p::reset_gameplay(&in.ui_state);
    in.ui_state.debug_visible = false;
    in.root_pose = runtime.root_pose.metrics;
    in.render_profile = p::RenderProfile::KnownGoodUnlit;
    in.lighting_mode = p::LightingMode::Off;
    in.fog_mode = p::FogMode::Off;
    in.shadow_mode = p::ShadowMode::Off;
    return in;
}

bool draw_gameplay(
    p::Runtime* runtime, p::RenderMetrics* metrics,
    const room::Vec3& link_pos, float link_yaw,
    const ViewState& camera_state,
    bool heart_visible, const room::Vec3& item_pos,
    float heart_scale,
    const p::MessageOverlayRenderInput* message_overlay = nullptr) {
    g_queue.begin_frame();
    if (!g_processes.draw_all()) return false;
    ModelCollector collector = {};
    if (!g_queue.flush(collect_model, &collector) || collector.count != 2) return false;
    if (heart_visible) {
        if (collector.count >= 4) return false;
        auto& heart = collector.views[collector.count++];
        heart.model = g_heart_model;
        // package lengths are encoded in the package headers and renderer also
        // receives the actual staged sizes through these globals below.
        std::uint32_t model_size = 0, texture_size = 0;
        char model_path[256] = {}, texture_path[256] = {};
        if (!path("heart.dprm", model_path) || !path("heart.dptx", texture_path)) return false;
        std::FILE* mf = std::fopen(model_path, "rb");
        std::FILE* tf = std::fopen(texture_path, "rb");
        if (mf == nullptr || tf == nullptr) { if (mf) std::fclose(mf); if (tf) std::fclose(tf); return false; }
        std::fseek(mf,0,SEEK_END); model_size=static_cast<std::uint32_t>(std::ftell(mf)); std::fclose(mf);
        std::fseek(tf,0,SEEK_END); texture_size=static_cast<std::uint32_t>(std::ftell(tf)); std::fclose(tf);
        heart.model_size = model_size;
        heart.textures = g_heart_textures;
        heart.texture_size = texture_size;
        identity_at(heart.matrix, item_pos.x, item_pos.y, item_pos.z);
        heart.scale[0] = heart.scale[1] = heart.scale[2] = heart_scale;
    }
    auto in = render_input(*runtime, link_pos, link_yaw, camera_state);
    in.message_overlay = message_overlay;
    return p::render_real_room_frame_with_models(
        *runtime, in, collector.views, collector.count, metrics);
}

void update_heart_pose(
    const p::Runtime& runtime, const room::Vec3& link_pos,
    float render_yaw, room::Vec3* item_pos, float* scale) {
    const auto& foot = runtime.animation.global[21];
    const room::Vec3 local_foot = {
        foot.value[0][3], foot.value[1][3], foot.value[2][3]};
    const room::Vec3 world_foot = link::transform_point(
        link::actor_matrix(link_pos, render_yaw), local_foot);
    float ox = 0.0f, oz = 0.0f;
    rotate_y(0.0f, 54.0f, render_yaw, &ox, &oz);
    *item_pos = {
        link_pos.x + ox, world_foot.y + 115.0f, link_pos.z + oz};
    *scale = std::min(1.0f, *scale + 0.35f);
}

}  // namespace

int main() {
    if (!dusk::psp::initialize({"Dusklight source treasure", kDir})) return 1;

    std::uint32_t manifest_size=0,lm=0,lt=0,la=0,ui=0,rm=0,rt=0,rs=0,ev=0,msg=0,hm=0,ht=0;
    if (!read_asset("RESOURCE.MANIFEST",g_manifest,sizeof(g_manifest),&manifest_size) ||
        !read_asset("link.dpsk",g_link_model,sizeof(g_link_model),&lm) ||
        !read_asset("link.dptx",g_link_textures,sizeof(g_link_textures),&lt) ||
        !read_asset("link-treasure.dpan",g_link_animation,sizeof(g_link_animation),&la) ||
        !read_asset("hud.dpui",g_ui,sizeof(g_ui),&ui) ||
        !read_asset("room.dprm",g_room_model,sizeof(g_room_model),&rm) ||
        !read_asset("room.dptx",g_room_textures,sizeof(g_room_textures),&rt) ||
        !read_asset("room.dpsc",g_room_scene,sizeof(g_room_scene),&rs) ||
        !read_asset("event_list.dat",g_event_list,sizeof(g_event_list),&ev) ||
        !read_asset("zel_00.bmg",g_message_bmg,sizeof(g_message_bmg),&msg) ||
        !read_asset("heart.dprm",g_heart_model,sizeof(g_heart_model),&hm) ||
        !read_asset("heart.dptx",g_heart_textures,sizeof(g_heart_textures),&ht)) return fail(10);

    p::PackageSet packages = {};
    room::PackageView scene = {}, heart_model = {};
    if (p::validate_dpsk(g_link_model,lm,&packages.model)!=p::PackageError::Ok ||
        p::validate_dptx(g_link_textures,lt,&packages.textures)!=p::PackageError::Ok ||
        p::validate_dpan(g_link_animation,la,&packages.animations)!=p::PackageError::Ok ||
        p::validate_dpui(g_ui,ui,&packages.ui)!=p::PackageError::Ok ||
        room::validate_dpsc(g_room_scene,rs,&scene)!=room::PackageError::Ok ||
        room::validate_dprm(g_heart_model,hm,&heart_model)!=room::PackageError::Ok ||
        !g_script.initialize(g_event_list,ev) ||
        !g_message_database.initialize(g_message_bmg,msg)) return fail(11);

    g_processes.initialize(); g_queue.initialize(); g_world.initialize();
    g_event_context.initialize(); g_interactions.initialize(); g_items.initialize();
    if (!g_resources.initialize(kDir,g_manifest,manifest_size,resource_reader,nullptr) ||
        !g_models.initialize(&g_resources,&g_queue,&g_world)) return fail(12);
    process::bind_process_manager(&g_processes); model::bind_model_runtime(&g_models);
    compat::bind_scene_exit_facade({&g_items,nullptr,nullptr,nullptr,nullptr,nullptr,treasure_query});
    events::bind_source_event_script(&g_script);
    if (!compat::bind_original_tbox_context(
            &g_processes,&g_event_context,&g_interactions,&g_items) ||
        !compat::register_original_tbox_profile(&g_processes)) return fail(13);

    process::ProcessHandle handles[2] = {};
    std::uint16_t created = 0;
    if (!compat::create_original_tboxes(&g_processes,scene,2,handles,2,&created) || created!=2) return fail(14);
    daTbox_c* heart_chest = nullptr;
    for (std::uint16_t i=0;i<created;++i) {
        auto* actor=static_cast<daTbox_c*>(g_processes.instance(handles[i]));
        if(actor!=nullptr && actor->getItemNo()==kHeart) heart_chest=actor;
    }
    if(heart_chest==nullptr) return fail(15);

    const std::int16_t initial_yaw_s16=static_cast<std::int16_t>(heart_chest->shape_angle.y+0x8000u);
    const float initial_yaw=link::s16_to_radians(static_cast<std::uint16_t>(initial_yaw_s16));
    const room::Vec3 link_pos={
        heart_chest->current.pos.x-111.0f*std::sin(initial_yaw),
        heart_chest->current.pos.y,
        heart_chest->current.pos.z-111.0f*std::cos(initial_yaw)};
    daPy_py_c* player=daPy_getPlayerActorClass();
    player->current.pos.set(link_pos.x,link_pos.y,link_pos.z);
    player->old.pos=player->current.pos;
    player->current.angle.y=initial_yaw_s16;
    player->shape_angle.y=initial_yaw_s16;
    player->attention_info.position.set(link_pos.x,link_pos.y+150.0f,link_pos.z);
    player->duskPspSetBaseAnimeFrame(0.0f);

    p::Runtime link_runtime = {};
    p::RenderMetrics render_metrics = {};
    p::PackageView room_tx={g_room_textures,rt,0,0};
    if(!p::initialize_runtime(&link_runtime,packages) ||
       !p::update_source_animation_and_skin(&link_runtime,p::Locomotion::Idle,0.0f,1.0f/30.0f) ||
       !p::initialize_real_room_renderer(
           packages.textures,room_tx,packages.ui,g_room_model,rm,
           g_command,sizeof(g_command),&render_metrics)) return fail(16);

    // Initial gameplay camera behind Link; fixedFrameEvCamera uses this solely
    // to choose the source 'n' side for its Eye vector.
    ViewState view={{link_pos.x,link_pos.y+105.0f,link_pos.z-20.0f},
                    {link_pos.x,link_pos.y+165.0f,link_pos.z+330.0f},52.0f};
    room::Vec3 item_pos={}; float heart_scale=0.0f;
    if(!draw_gameplay(&link_runtime,&render_metrics,link_pos,initial_yaw,view,false,item_pos,0.0f) ||
       !write_marker("CHEST_CLOSED.VISIBLE","DUSKLIGHT_PSP_CHEST_SOURCE_CLOSED\n")) return fail(17);
    dusk::psp::sleep_microseconds(700000);

    // Source actor exposes OPEN from boxCheck(), then starts event 0x020D.
    if(!g_processes.execute_all() ||
       !compat::sample_original_tbox_interaction(&g_processes,false) ||
       !g_interactions.available() ||
       !compat::sample_original_tbox_interaction(&g_processes,true) ||
       g_script.active_event_id()!=static_cast<std::int16_t>(0x020D)) return fail(18);

    const int link_staff=g_script.staff_id("Link",0);
    const int camera_staff=g_script.staff_id("CAMERA",0);
    const int treasure_staff=g_script.staff_id("TREASURE",0);
    if(link_staff<0 || camera_staff<0 || treasure_staff<0) return fail(19);

    float link_frame=0.0f;
    bool geta_phase=false,getawait_phase=false,message_ack=false;
    bool opening_marker=false,item_marker=false,message_marker=false;
    std::uint32_t getawait_frames=0,final_pause_frames=0,message_ready_hold=0;
    std::uint32_t partner=fpcM_ERROR_PROCESS_ID_e;
    camera::SourceGetItemCamera getitem_camera;
    bool getitem_camera_started=false;
    std::uint32_t fixed_timer=0;

    for(std::uint32_t safety=0;safety<520 && !g_script.completed();++safety) {
        const char* link_cut=g_script.current_cut_name(link_staff);
        const char* camera_cut=g_script.current_cut_name(camera_staff);
        if(link_cut==nullptr || camera_cut==nullptr) return fail(20);

        float render_yaw=initial_yaw;
        bool heart_visible=false;

        if(std::strcmp(link_cut,"058lchange")==0) {
            if(g_script.staff_advanced(link_staff) && !g_script.cut_end(link_staff)) return fail(21);
        } else if(std::strcmp(link_cut,"010open_treasure")==0) {
            if(g_script.staff_advanced(link_staff)) link_frame=0.0f;
            player->duskPspSetBaseAnimeFrame(link_frame);
            if(!p::apply_source_animation_resource_and_skin(&link_runtime,kBoxop,link_frame)) return fail(22);
            if(!opening_marker && link_frame>=28.0f) {
                opening_marker=true;
                if(!write_marker("CHEST_OPENING.VISIBLE","DUSKLIGHT_PSP_CHEST_SOURCE_OPENING\n")) return fail(23);
            }
            if(link_frame>=111.0f) {
                if(!g_script.cut_end(link_staff)) return fail(24);
            } else link_frame+=1.0f;
        } else if(std::strcmp(link_cut,"011get_item")==0) {
            if(g_script.staff_advanced(link_staff)) {
                geta_phase=true; getawait_phase=false; link_frame=0.0f; getawait_frames=0;
                partner=g_event_context.item_partner();
                std::uint8_t pending=0xff;
                if(partner==fpcM_ERROR_PROCESS_ID_e ||
                   !compat::original_tbox_demo_item_pending(partner,&pending) || pending!=kHeart ||
                   g_items.quantity(kHeart)!=0 ||
                   !g_message_runtime.begin(&g_message_database,0x86)) return fail(25);
            }
            if(geta_phase) {
                const std::int16_t yaw_s16=source_geta_yaw(initial_yaw_s16,link_frame);
                render_yaw=link::s16_to_radians(static_cast<std::uint16_t>(yaw_s16));
                player->shape_angle.y=yaw_s16;
                player->duskPspSetBaseAnimeFrame(link_frame);
                if(!p::apply_source_animation_resource_and_skin(&link_runtime,kGetA,link_frame)) return fail(26);
                if(link_frame>=25.0f && !compat::original_tbox_demo_item_visible(partner) &&
                   !compat::original_tbox_demo_item_show(partner)) return fail(27);
                heart_visible=compat::original_tbox_demo_item_visible(partner);
                if(heart_visible) update_heart_pose(
                    link_runtime,link_pos,render_yaw,&item_pos,&heart_scale);
                if(link_frame>=30.0f) { geta_phase=false; getawait_phase=true; link_frame=0.0f; }
                else link_frame+=1.0f;
            } else if(getawait_phase) {
                const std::int16_t yaw_s16=static_cast<std::int16_t>(initial_yaw_s16+0x8000u);
                render_yaw=link::s16_to_radians(static_cast<std::uint16_t>(yaw_s16));
                player->shape_angle.y=yaw_s16;
                player->current.angle.y=yaw_s16;
                const float frame=static_cast<float>(getawait_frames%31u);
                player->duskPspSetBaseAnimeFrame(frame);
                if(!p::apply_source_animation_resource_and_skin(&link_runtime,kGetAWait,frame)) return fail(28);
                ++getawait_frames;
                heart_visible=compat::original_tbox_demo_item_visible(partner);
                if(heart_visible) update_heart_pose(
                    link_runtime,link_pos,render_yaw,&item_pos,&heart_scale);
                if(heart_visible && !item_marker && getawait_frames>=10u) {
                    item_marker=true;
                    if(g_items.quantity(kHeart)!=0 ||
                       !write_marker("CHEST_ITEM.VISIBLE",
                        "DUSKLIGHT_PSP_CHEST_SOURCE_HEART_VISIBLE_BEFORE_COMMIT\n")) return fail(29);
                }
            }
        } else if(std::strcmp(link_cut,"001n_wait")==0) {
            if(g_script.staff_advanced(link_staff) && !g_script.cut_end(link_staff)) return fail(31);
            render_yaw=link::s16_to_radians(static_cast<std::uint16_t>(player->shape_angle.y));
        } else return fail(32);

        if(g_message_runtime.active()) {
            const bool ready=g_message_runtime.awaiting_confirm();
            if(ready) {
                if(!message_marker) {
                    message_marker=true;
                    message_ready_hold=0;
                    if(g_items.quantity(kHeart)!=0 ||
                       !write_marker("CHEST_MESSAGE.VISIBLE",
                        "DUSKLIGHT_PSP_CHEST_SOURCE_MESSAGE_0086_VISIBLE\n")) return fail(301);
                }
                ++message_ready_hold;
            }
            // Fixture-only input injection: hold a stable fully-rendered message
            // window for screenshot evidence, then issue one deliberate Action.
            // The SourceMessageRuntime itself contains no autonomous timeout.
            const bool action_pressed=ready && message_ready_hold>=45u;
            if(!g_message_runtime.tick(action_pressed)) return fail(302);
            if(g_message_runtime.acknowledged() && !message_ack) {
                if(!compat::original_tbox_demo_item_kill_and_commit(partner) ||
                   g_items.quantity(kHeart)!=1 || !g_script.cut_end(link_staff)) return fail(30);
                message_ack=true;
                heart_visible=false;
            }
        }

        // Camera staff from the same source script.
        if(std::strcmp(camera_cut,"WAIT")==0) {
            if(g_script.staff_advanced(camera_staff) && !g_script.cut_end(camera_staff)) return fail(33);
        } else if(std::strcmp(camera_cut,"FIXEDFRM")==0) {
            if(g_script.staff_advanced(camera_staff)) {
                fixed_timer=0;
                const auto* c=g_script.vector_data(camera_staff,"Center");
                const auto* e=g_script.vector_data(camera_staff,"Eye");
                const float* f=g_script.float_data(camera_staff,"Fovy");
                const std::int32_t* timer=g_script.integer_data(camera_staff,"Timer");
                const char* mask=g_script.string_data(camera_staff,"RelUseMask");
                if(c==nullptr || e==nullptr || f==nullptr || timer==nullptr ||
                   mask==nullptr || std::strcmp(mask,"on")!=0) return fail(34);
                const room::Vec3 chest_pos={heart_chest->current.pos.x,heart_chest->current.pos.y,heart_chest->current.pos.z};
                view.center=relative_to_actor(chest_pos,heart_chest->shape_angle.y,*c);
                events::SourceEventVec3 adjusted=*e;
                // fixedFrame 'n': choose X sign from current Eye relative to actor.
                const float actor_yaw=link::s16_to_radians(static_cast<std::uint16_t>(heart_chest->shape_angle.y));
                const float dx=view.eye.x-chest_pos.x,dz=view.eye.z-chest_pos.z;
                const float current_u=std::atan2(dx,dz);
                float relative=current_u-actor_yaw;
                while(relative>3.14159265f) relative-=kTau;
                while(relative<-3.14159265f) relative+=kTau;
                if(relative<0.0f) adjusted.x=-adjusted.x;
                view.eye=relative_to_actor(chest_pos,heart_chest->shape_angle.y,adjusted);
                view.fov=*f;
            }
            const std::int32_t* timer=g_script.integer_data(camera_staff,"Timer");
            if(timer==nullptr) return fail(35);
            if(fixed_timer++>=static_cast<std::uint32_t>(std::max<std::int32_t>(0,*timer)) &&
               !g_script.cut_end(camera_staff)) return fail(36);
        } else if(std::strcmp(camera_cut,"PAUSE")==0) {
            const std::int32_t* timer=g_script.integer_data(camera_staff,"Timer");
            const std::int32_t* wait_key=g_script.integer_data(camera_staff,"WaitAnyKey");
            if(g_script.staff_advanced(camera_staff)) final_pause_frames=0;
            if(timer==nullptr) {
                if(!g_script.cut_end(camera_staff)) return fail(37);
            } else {
                ++final_pause_frames;
                const bool timer_done=final_pause_frames>=static_cast<std::uint32_t>(std::max<std::int32_t>(0,*timer));
                const bool key_done=wait_key==nullptr || *wait_key==0 || message_ack;
                if(timer_done && key_done && !g_script.cut_end(camera_staff)) return fail(38);
            }
        } else if(std::strcmp(camera_cut,"GETITEM")==0) {
            if(g_script.staff_advanced(camera_staff)) {
                const std::int32_t* type=g_script.integer_data(camera_staff,"Type");
                if(type==nullptr || *type!=2) return fail(39);
                const camera::SourceGetItemCameraInput input={
                    {view.center.x,view.center.y,view.center.z},
                    {view.eye.x,view.eye.y,view.eye.z},view.fov,
                    {link_pos.x,link_pos.y,link_pos.z},
                    {link_pos.x,link_pos.y+150.0f,link_pos.z},
                    initial_yaw_s16,*type,false,false};
                if(!getitem_camera.begin(input)) return fail(40);
                getitem_camera_started=true;
            }
            if(!getitem_camera_started) return fail(41);
            if(getitem_camera.active() && !getitem_camera.step()) return fail(41);
            const auto& cv=getitem_camera.view();
            view.center={cv.center.x,cv.center.y,cv.center.z};
            view.eye={cv.eye.x,cv.eye.y,cv.eye.z}; view.fov=cv.fov;
            // The camera can finish before Link's 011get_item cut. Source
            // event dependencies then keep the current GETITEM cut selected;
            // hold the final view and re-emit cutEnd until flag 298 unlocks
            // the following PAUSE rather than restarting the camera.
            if(cv.finished && !g_script.cut_end(camera_staff)) return fail(42);
        } else return fail(43);

        // Original chest actor consumes TREASURE cut and synchronizes its BCK
        // to Link's source animation frame through getBaseAnimeFrame().
        if(!g_processes.execute_all()) return fail(44);

        p::MessageOverlayRenderInput overlay = {};
        const p::MessageOverlayRenderInput* overlay_ptr = nullptr;
        if(g_message_runtime.active()) {
            const auto& source_message=g_message_runtime.message();
            overlay.text=source_message.text;
            overlay.source_message_id=source_message.id;
            overlay.visible_characters=g_message_runtime.revealed_characters();
            overlay.active=true;
            overlay.awaiting_confirm=g_message_runtime.awaiting_confirm();
            overlay_ptr=&overlay;
        }
        if(!draw_gameplay(
                &link_runtime,&render_metrics,link_pos,render_yaw,view,
                heart_visible,item_pos,heart_scale,overlay_ptr)) return fail(45);

        g_script.tick();
        dusk::psp::sleep_microseconds(33333);
    }

    if(!g_script.completed() || !message_ack || !opening_marker || !item_marker || !message_marker ||
       g_items.quantity(kHeart)!=1 || !g_items.is_treasure_open(19)) return fail(50);

    // Let the original chest observe endCheck(), reset the event, and settle.
    if(!g_processes.execute_all() || g_event_context.state()!=events::State::None) return fail(51);
    if(!p::update_source_animation_and_skin(&link_runtime,p::Locomotion::Idle,0.0f,0.0f)) return fail(52);
    if(!draw_gameplay(&link_runtime,&render_metrics,link_pos,
            link::s16_to_radians(static_cast<std::uint16_t>(player->shape_angle.y)),
            view,false,item_pos,0.0f)) return fail(53);
    if(!write_marker("CHEST_COMPLETE.OK",
        "DUSKLIGHT_PSP_CHEST_SOURCE_FULL_OK item=0x21 treasure=19 deferred_commit=1 source_message=0x86 input_gated=1\n")) return fail(54);

    // Persistence: recreate both source TBoxes from the same DPSC and verify
    // Heart Piece chest remains opened without creating/committing a second item.
    const auto before=compat::original_tbox_metrics();
    g_processes.destroy_room(2); compat::deactivate_original_tboxes(2);
    process::ProcessHandle second_handles[2]={}; std::uint16_t second_created=0;
    if(!compat::create_original_tboxes(&g_processes,scene,2,second_handles,2,&second_created) ||
       second_created!=2 || g_items.quantity(kHeart)!=1 || !g_items.is_treasure_open(19)) return fail(55);
    const auto after=compat::original_tbox_metrics();
    if(after.items_created!=before.items_created || after.items_committed!=before.items_committed) return fail(56);
    if(!write_marker("CHEST_PERSISTENCE.OK","DUSKLIGHT_PSP_CHEST_SOURCE_PERSISTENCE_OK\n")) return fail(57);

    dusk::psp::sleep_microseconds(1800000);
    p::shutdown_renderer();
    compat::unbind_original_tbox_context(); events::unbind_source_event_script();
    compat::unbind_scene_exit_facade(); model::unbind_model_runtime(); process::unbind_process_manager();
    g_models.shutdown();g_resources.shutdown();g_items.shutdown();g_interactions.shutdown();g_event_context.shutdown();g_world.shutdown();g_queue.shutdown();
    dusk::psp::shutdown();sceKernelExitGame();return 0;
}
