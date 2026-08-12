#include "dusk/psp/link_fidelity.hpp"
#include "dusk/psp/platform.hpp"
#include "dusk/psp/playable_package.hpp"
#include "dusk/psp/playable_render.hpp"
#include "dusk/psp/playable_runtime.hpp"
#include "dusk/psp/presentation_profile.hpp"
#include "dusk/psp/room_package.hpp"

#include <pspkernel.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

PSP_MODULE_INFO("DuskGetAwaitHeart", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-256);

namespace p = dusk::psp::playable;
namespace r = dusk::psp::room;
namespace link = dusk::psp::link;

namespace {
constexpr char kDir[] = "ms0:/PSP/GAME/DUSKLIGHT_GETAWAIT_HEART";
constexpr std::uint32_t kGetAWait = 0x16A;
constexpr std::uint32_t kTboxHash = 0x2A0E83C6u;
constexpr std::uint8_t kHeartPiece = 0x21u;
constexpr std::uint32_t kCommandBytes = 512u * 1024u;
constexpr std::uint32_t kFramePixels = 512u * 272u;

alignas(16) std::uint8_t g_link_model[300000];
alignas(16) std::uint8_t g_link_textures[460000];
alignas(16) std::uint8_t g_link_animation[300000];
alignas(16) std::uint8_t g_ui[160000];
alignas(16) std::uint8_t g_room_model[700000];
alignas(16) std::uint8_t g_room_textures[500000];
alignas(16) std::uint8_t g_room_scene[24000];
alignas(16) std::uint8_t g_heart_model[40000];
alignas(16) std::uint8_t g_heart_textures[4000];
alignas(16) std::uint8_t g_command[kCommandBytes];
alignas(16) std::uint16_t g_without[kFramePixels];
alignas(16) std::uint16_t g_with[kFramePixels];

bool make_path(const char* leaf, char out[256]) {
    return dusk::psp::make_game_path(leaf, out, 256);
}
bool read_asset(const char* leaf, void* dst, std::uint32_t cap, std::uint32_t* size) {
    char path[256] = {};
    return make_path(leaf, path) && dusk::psp::read_file(path, dst, cap, size);
}
bool write_marker(const char* leaf, const char* text) {
    char path[256] = {};
    return make_path(leaf, path) && dusk::psp::write_file(path, text, static_cast<std::uint32_t>(std::strlen(text)));
}
int fail(int code) {
    char text[48] = {};
    std::snprintf(text, sizeof(text), "code=%d\n", code);
    write_marker("GETAWAIT_HEART.FAIL", text);
    dusk::psp::sleep_microseconds(1500000);
    p::shutdown_renderer(); dusk::psp::shutdown(); sceKernelExitGame(); return code;
}
void identity_at(float m[12], float x, float y, float z) {
    std::memset(m, 0, sizeof(float) * 12); m[0]=m[5]=m[10]=1.0f; m[3]=x; m[7]=y; m[11]=z;
}
void rotate_y(float x, float z, float yaw, float* ox, float* oz) {
    const float s=std::sin(yaw), c=std::cos(yaw); *ox=x*c+z*s; *oz=z*c-x*s;
}
std::uint32_t differences(int* minx,int* miny,int* maxx,int* maxy) {
    std::uint32_t n=0;*minx=480;*miny=272;*maxx=-1;*maxy=-1;
    for(int y=0;y<272;++y) for(int x=0;x<480;++x) {
        const std::uint32_t i=static_cast<std::uint32_t>(y)*512u+static_cast<std::uint32_t>(x);
        if(g_without[i]!=g_with[i]) {++n;if(x<*minx)*minx=x;if(y<*miny)*miny=y;if(x>*maxx)*maxx=x;if(y>*maxy)*maxy=y;}
    }
    return n;
}
}

int main() {
    if(!dusk::psp::initialize({"Dusklight GETAWAIT Heart",kDir})) return 1;
    std::uint32_t lm=0,lt=0,la=0,ui=0,rm=0,rt=0,rs=0,hm=0,ht=0;
    if(!read_asset("link.dpsk",g_link_model,sizeof(g_link_model),&lm) ||
       !read_asset("link.dptx",g_link_textures,sizeof(g_link_textures),&lt) ||
       !read_asset("link-item-get.dpan",g_link_animation,sizeof(g_link_animation),&la) ||
       !read_asset("hud.dpui",g_ui,sizeof(g_ui),&ui) ||
       !read_asset("room.dprm",g_room_model,sizeof(g_room_model),&rm) ||
       !read_asset("room.dptx",g_room_textures,sizeof(g_room_textures),&rt) ||
       !read_asset("room.dpsc",g_room_scene,sizeof(g_room_scene),&rs) ||
       !read_asset("heart.dprm",g_heart_model,sizeof(g_heart_model),&hm) ||
       !read_asset("heart.dptx",g_heart_textures,sizeof(g_heart_textures),&ht)) return fail(10);

    p::PackageSet pkg={};
    if(p::validate_dpsk(g_link_model,lm,&pkg.model)!=p::PackageError::Ok ||
       p::validate_dptx(g_link_textures,lt,&pkg.textures)!=p::PackageError::Ok ||
       p::validate_dpan(g_link_animation,la,&pkg.animations)!=p::PackageError::Ok ||
       p::validate_dpui(g_ui,ui,&pkg.ui)!=p::PackageError::Ok) return fail(11);
    r::PackageView scene={},heart_model={};
    if(r::validate_dpsc(g_room_scene,rs,&scene)!=r::PackageError::Ok ||
       r::validate_dprm(g_heart_model,hm,&heart_model)!=r::PackageError::Ok) return fail(12);

    r::SceneActorV3 chest={}; bool found=false;
    const std::uint32_t actors=r::read_u32(scene.bytes+136);
    for(std::uint32_t i=0;i<actors;++i) {
        r::SceneActorV3 a={}; if(r::read_dpsc_actor_v3(scene,i,&a)!=r::PackageError::Ok) return fail(13);
        const std::uint8_t item=static_cast<std::uint8_t>((static_cast<std::uint16_t>(a.rotation[2])>>8)&0xffu);
        if(a.name_hash==kTboxHash && item==kHeartPiece) {chest=a;found=true;break;}
    }
    if(!found) return fail(14);

    // procCoOpenTreasureInit(): initial Link yaw faces the chest.
    const std::int16_t initial_yaw_s16=static_cast<std::int16_t>(chest.rotation[1]+0x8000u);
    const float initial_yaw=link::s16_to_radians(static_cast<std::uint16_t>(initial_yaw_s16));
    const r::Vec3 link_pos={
        chest.position[0]-111.0f*std::sin(initial_yaw), chest.position[1],
        chest.position[2]-111.0f*std::cos(initial_yaw)};
    // procCoGetItemInit() reached from PROC_OPEN_TREASURE sets
    // mProcVar3=-0x8000. procCoGetItem() rotates shape_angle from frame 9
    // through 16, so GETAWAIT begins exactly 180 degrees from the initial yaw.
    const std::int16_t getawait_yaw_s16=static_cast<std::int16_t>(initial_yaw_s16+0x8000u);
    const float getawait_yaw=link::s16_to_radians(static_cast<std::uint16_t>(getawait_yaw_s16));

    p::Runtime runtime={}; if(!p::initialize_runtime(&runtime,pkg)) return fail(15);
    p::RenderMetrics metrics={}; p::PackageView room_tx={g_room_textures,rt,0,0};
    if(!p::initialize_real_room_renderer(pkg.textures,room_tx,pkg.ui,g_room_model,rm,g_command,sizeof(g_command),&metrics)) return fail(16);

    struct Best {std::uint32_t diff=0;int frame=0;int side=0;int minx=0,miny=0,maxx=0,maxy=0;float itemx=0,itemy=0,itemz=0;float footy=0;} best;
    for(int side=-1;side<=1;side+=2) {
        for(int frame=0;frame<=30;frame+=3) {
            if(!p::apply_source_animation_resource_and_skin(&runtime,kGetAWait,static_cast<float>(frame))) return fail(20);
            const auto& foot=runtime.animation.global[21];
            const r::Vec3 local_foot={foot.value[0][3],foot.value[1][3],foot.value[2][3]};
            const r::Vec3 world_foot=link::transform_point(link::actor_matrix(link_pos,getawait_yaw),local_foot);
            float ox=0,oz=0;rotate_y(0.0f,54.0f,getawait_yaw,&ox,&oz);
            const r::Vec3 item={link_pos.x+ox,world_foot.y+115.0f,link_pos.z+oz};
            // daAlink_c::setAttentionPos(): ordinary human Link uses the
            // source normalOffset (0,150,0). getItemEvCamera::relationalPos()
            // is relative to attention_info.position, not current.pos + 90.
            const r::Vec3 attention={link_pos.x,link_pos.y+150.0f,link_pos.z};
            // GETITEM target is captured at cut start, before GETA turns Link.
            float cx=0,cz=0,ex=0,ez=0;rotate_y(0.0f,-62.0f,initial_yaw,&cx,&cz);rotate_y(84.0f*static_cast<float>(side),-164.0f,initial_yaw,&ex,&ez);
            p::RealRoomRenderInput in={};in.link_position={link_pos.x,link_pos.y,link_pos.z};in.link_yaw=getawait_yaw;
            in.camera_center={attention.x+cx,attention.y-27.0f,attention.z+cz};in.camera_eye={attention.x+ex,attention.y-18.0f,attention.z+ez};in.camera_fov=50.0f;
            in.interaction={0,0,0};in.presentation=dusk::psp::presentation::Profile::Game;p::reset_gameplay(&in.ui_state);in.ui_state.debug_visible=false;
            in.root_pose=runtime.root_pose.metrics;in.environment=nullptr;in.shadows=nullptr;in.render_profile=p::RenderProfile::KnownGoodUnlit;in.lighting_mode=p::LightingMode::Off;in.fog_mode=p::FogMode::Off;in.shadow_mode=p::ShadowMode::Off;
            p::StaticModelRenderView heart={};heart.model=g_heart_model;heart.model_size=hm;heart.textures=g_heart_textures;heart.texture_size=ht;identity_at(heart.matrix,item.x,item.y,item.z);heart.scale[0]=heart.scale[1]=heart.scale[2]=1.0f;
            if(!p::render_real_room_frame_with_models(runtime,in,&heart,0,&metrics) || !p::capture_playable_frame_5650(g_without,sizeof(g_without))) return fail(21);
            if(!p::render_real_room_frame_with_models(runtime,in,&heart,1,&metrics) || !p::capture_playable_frame_5650(g_with,sizeof(g_with))) return fail(22);
            int minx,miny,maxx,maxy;const std::uint32_t diff=differences(&minx,&miny,&maxx,&maxy);
            if(diff>best.diff) best={diff,frame,side,minx,miny,maxx,maxy,item.x,item.y,item.z,world_foot.y};
        }
    }
    if(best.diff==0) return fail(30);

    // Leave the best source pose/camera combination visible for visual inspection.
    if(!p::apply_source_animation_resource_and_skin(&runtime,kGetAWait,static_cast<float>(best.frame))) return fail(31);
    const auto& foot=runtime.animation.global[21];const r::Vec3 local_foot={foot.value[0][3],foot.value[1][3],foot.value[2][3]};const r::Vec3 world_foot=link::transform_point(link::actor_matrix(link_pos,getawait_yaw),local_foot);
    float ox=0,oz=0;rotate_y(0,54,getawait_yaw,&ox,&oz);const r::Vec3 item={link_pos.x+ox,world_foot.y+115,link_pos.z+oz};const r::Vec3 attention={link_pos.x,link_pos.y+150,link_pos.z};float cx=0,cz=0,ex=0,ez=0;rotate_y(0,-62,initial_yaw,&cx,&cz);rotate_y(84.0f*best.side,-164,initial_yaw,&ex,&ez);
    p::RealRoomRenderInput in={};in.link_position={link_pos.x,link_pos.y,link_pos.z};in.link_yaw=getawait_yaw;in.camera_center={attention.x+cx,attention.y-27,attention.z+cz};in.camera_eye={attention.x+ex,attention.y-18,attention.z+ez};in.camera_fov=50;in.presentation=dusk::psp::presentation::Profile::Game;p::reset_gameplay(&in.ui_state);in.ui_state.debug_visible=false;in.root_pose=runtime.root_pose.metrics;in.render_profile=p::RenderProfile::KnownGoodUnlit;in.lighting_mode=p::LightingMode::Off;in.fog_mode=p::FogMode::Off;in.shadow_mode=p::ShadowMode::Off;
    p::StaticModelRenderView heart={};heart.model=g_heart_model;heart.model_size=hm;heart.textures=g_heart_textures;heart.texture_size=ht;identity_at(heart.matrix,item.x,item.y,item.z);heart.scale[0]=heart.scale[1]=heart.scale[2]=1;
    for(int i=0;i<3;++i) if(!p::render_real_room_frame_with_models(runtime,in,&heart,1,&metrics)) return fail(32);
    char report[768]={};std::snprintf(report,sizeof(report),
        "DUSKLIGHT_PSP_GETAWAIT_HEART_VISIBLE_OK\nsource_clip=GETAWAIT\nresource_id=0x16A\nsource_bmd=o_gd_hutk.bmd\nsource_triangles=484\nchest=%.3f,%.3f,%.3f\nlink=%.3f,%.3f,%.3f\ninitial_yaw_s16=%d\ngetawait_yaw_s16=%d\nbest_frame=%d\ncamera_side=%d\nleft_foot_y=%.3f\nitem=%.3f,%.3f,%.3f\npixel_differences=%lu\ndiff_bounds=%d,%d,%d,%d\n",
        chest.position[0],chest.position[1],chest.position[2],link_pos.x,link_pos.y,link_pos.z,static_cast<int>(initial_yaw_s16),static_cast<int>(getawait_yaw_s16),best.frame,best.side,best.footy,best.itemx,best.itemy,best.itemz,static_cast<unsigned long>(best.diff),best.minx,best.miny,best.maxx,best.maxy);
    if(!write_marker("GETAWAIT_HEART.OK",report)) return fail(33);
    dusk::psp::sleep_microseconds(5000000);p::shutdown_renderer();dusk::psp::shutdown();sceKernelExitGame();return 0;
}
