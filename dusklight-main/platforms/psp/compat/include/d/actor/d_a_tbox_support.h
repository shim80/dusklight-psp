#ifndef DUSK_PSP_COMPAT_D_A_TBOX_SUPPORT_H
#define DUSK_PSP_COMPAT_D_A_TBOX_SUPPORT_H

// Exact source-facing names used by d_a_tbox.cpp. This is a bounded PSP
// compatibility surface, not a GameCube ABI declaration.

#include "d/d_com_inf_game.h"
#include "d/actor/d_a_player.h"
#include "m_Do/m_Do_mtx.h"

struct cM3dGCylS {
    cXyz mCenter;
    f32 mRadius;
    f32 mHeight;

    constexpr cM3dGCylS() = default;
    constexpr cM3dGCylS(cXyz center, f32 radius, f32 height)
        : mCenter(center), mRadius(radius), mHeight(height) {}
    constexpr cM3dGCylS(
        f32 x, f32 y, f32 z, f32 radius, f32 height)
        : mCenter(x, y, z), mRadius(radius), mHeight(height) {}
};

class cM3dGCyl {
public:
    void Set(const cM3dGCylS& source) { source_ = source; }

private:
    cM3dGCylS source_ = {};
};

struct dCcD_SrcObjCommon {
    u32 parameter;
};
struct dCcD_SrcObjAt {
    u32 type;
    u8 attack_power;
    dCcD_SrcObjCommon common;
};
struct dCcD_SrcObjTg {
    u32 type;
    dCcD_SrcObjCommon common;
};
struct dCcD_SrcObjCo {
    dCcD_SrcObjCommon common;
};
struct dCcD_SrcObjHitInf {
    dCcD_SrcObjAt attack;
    dCcD_SrcObjTg target;
    dCcD_SrcObjCo collision;
};
struct dCcD_SrcObj {
    int flags;
    dCcD_SrcObjHitInf hit;
};
struct dCcD_SrcCommon {
    u32 value;
};
struct dCcD_SrcAtTg {
    u8 sound;
    u8 hit_mark;
    u8 special;
    u8 material;
    dCcD_SrcCommon common;
};
struct dCcD_SrcCo {
    dCcD_SrcCommon common;
};
struct dCcD_SrcGObjInf {
    dCcD_SrcObj object;
    dCcD_SrcAtTg attack;
    dCcD_SrcAtTg target;
    dCcD_SrcCo collision;
};
struct dCcD_SrcCylAttr {
    cM3dGCylS mCyl;
};
struct dCcD_SrcCyl {
    dCcD_SrcGObjInf object_info;
    dCcD_SrcCylAttr cylinder;
};

enum : u8 {
    dCcD_SE_NONE = 0,
    dCcD_SE_METAL = 2,
};

class dCcD_Stts {
public:
    void Init(int, int, fopAc_ac_c*) {}
};

class dCcD_Cyl : public cM3dGCyl {
public:
    void Set(const dCcD_SrcCyl&) {}
    void SetStts(dCcD_Stts*) {}
    void SetC(const cXyz&) {}
};

class dCcS_CompatWorld {
public:
    void Set(dCcD_Cyl*) {}
};

dCcS_CompatWorld* dComIfG_Ccsp();

struct dBgS_GndChk {
    void SetActorPid(fpc_ProcID) {}
    void SetPos(const cXyz*) {}
    int GetBgIndex() const { return -1; }
};
using dBgS_ObjGndChk = dBgS_GndChk;

class dBgS_AcchCir {
public:
    void SetWall(f32, f32) {}
};

class dBgS_ObjAcch {
public:
    dBgS_GndChk m_gnd;

    void Set(
        cXyz*, cXyz*, fopAc_ac_c*, int, dBgS_AcchCir*,
        cXyz*, csXyz*, csXyz*) {}
    bool ChkGroundLanding() const { return false; }
    bool ChkGroundHit() const { return false; }
    void ClrGroundHit() {}
    void CrrPos(dBgS_CompatWorld&) {}
};

struct GXColor { u8 r; u8 g; u8 b; u8 a; };

struct GXColorS10 {
    s16 r;
    s16 g;
    s16 b;
    s16 a;
};

struct LIGHT_INFLUENCE {
    cXyz mPosition;
    GXColorS10 mColor;
    f32 mPow;
    f32 mFluctuation;
    int mIndex;
};

void dKy_efplight_cut(LIGHT_INFLUENCE*);
void dKy_efplight_set(LIGHT_INFLUENCE*);
void dKy_set_allcol_ratio(f32);

struct dPnt {
    u8 mArg1;
    u8 mArg2;
    u8 mArg3;
    u8 mArg0;
    cXyz m_position;
};

struct dPath {
    u16 m_num;
    u16 m_nextID;
    u8 field_0x4;
    bool m_closed;
    u8 swbit;
    u8 field_0x7;
    dPnt* m_points;
};

dPath* dPath_GetRoomPath(int path_index, int room_no);

class dTres_c {
public:
    static void onStatus(int, int, int);
    static void offStatus(int, int, int);
    static void setPosition(int, const cXyz*);
};

class dItem_data {
public:
    static bool chkFlag(u8, u32);
    static const char* getArcName(u8);
    static s16 getBmdName(u8);
    static s16 getBtkName(u8);
    static s16 getBpkName(u8);
    static s16 getBckName(u8);
    static s16 getBxaName(u8);
    static s16 getBrkName(u8);
    static s16 getBtpName(u8);
    static s8 getTevFrm(u8);
    static s8 getBtpFrm(u8);
    static u8 getShadowSize(u8);
    static u8 getH(u8);
    static u8 getR(u8);
};

enum : u8 {
    dItemNo_GREEN_RUPEE_e = 0x01,
    dItemNo_BOMB_5_e = 0x0A,
    dItemNo_BOMB_10_e = 0x0B,
    dItemNo_BOMB_20_e = 0x0C,
    dItemNo_BOMB_30_e = 0x0D,
    dItemNo_WATER_BOMB_5_e = 0x16,
    dItemNo_WATER_BOMB_10_e = 0x17,
    dItemNo_WATER_BOMB_20_e = 0x18,
    dItemNo_WATER_BOMB_30_e = 0x19,
    dItemNo_BOMB_INSECT_5_e = 0x1A,
    dItemNo_BOMB_INSECT_10_e = 0x1B,
    dItemNo_BOMB_INSECT_20_e = 0x1C,
    dItemNo_BOMB_INSECT_30_e = 0x1D,
    dItemNo_ORANGE_RUPEE_e = 0x06,
    dItemNo_SILVER_RUPEE_e = 0x07,
    dItemNo_KAKERA_HEART_e = 0x21,
    dItemNo_UTAWA_HEART_e = 0x22,
    dItemNo_MAP_e = 0x23,
    dItemNo_COMPUS_e = 0x24,
    dItemNo_DUNGEON_EXIT_e = 0x25,
    dItemNo_DUNGEON_BACK_e = 0x27,
    dItemNo_DUNGEON_EXIT_2_e = 0x33,
    dItemNo_WALLET_LV3_e = 0x36,
    dItemNo_WOOD_STICK_e = 0x3F,
    dItemNo_BOOMERANG_e = 0x40,
    dItemNo_NORMAL_BOMB_e = 0x70,
    dItemNo_WATER_BOMB_e = 0x71,
    dItemNo_POKE_BOMB_e = 0x72,
    dItemNo_BOMB_BAG_LV1_e = 0x73,
    dItemNo_FAIRY_DROP_e = 0x73,
    dItemNo_DROP_BOTTLE_e = 0x75,
    dItemNo_CHUCHU_RARE_e = 0x77,
    dItemNo_POU_SPIRIT_e = 0xE0,
    dItemNo_LV7_DUNGEON_EXIT_e = 0xEC,
    dItemNo_NONE_e = 0xFF,
    SLOT_15 = 0x0F,
};

class daMidna_c : public fopAc_ac_c {
public:
    bool checkMetamorphoseEnable() const { return true; }
};

class dCamera_c {
public:
    void Stop() {}
    void Start() {}
    void SetTrimSize(u8) {}
    void Set(const cXyz&, const cXyz&, s16, f32) {}
};

class camera_process_class {
public:
    dCamera_c mCamera;
};

struct stage_camera2_data_class {
    u8 field_0x00[16];
    u8 m_arrow_idx;
    u8 field_0x11;
};
struct stage_camera_class {
    stage_camera2_data_class* m_entries;
};
struct stage_arrow_data_class {
    f32 posX;
    f32 posY;
    f32 posZ;
    s16 angleX;
};
struct stage_arrow_class {
    stage_arrow_data_class* m_entries;
};
class dStage_roomDt_c {
public:
    stage_camera_class* getCamera() { return camera_; }
    stage_arrow_class* getArrow() { return arrow_; }

private:
    stage_camera_class* camera_ = nullptr;
    stage_arrow_class* arrow_ = nullptr;
};

#pragma pack(push, 1)
struct dStage_MapEvent_dt_c {
    u8 type;
    u8 field_0x1;
    u8 field_0x2[4];
    u8 priority;
    u8 field_0x7[6];
    union {
        char event_name[13];
        struct {
            u8 field_0xd[7];
            u16 field_0x14;
            u8 field_0x16;
            u8 field_0x17;
            u8 sound_type;
            u8 field_0x19;
        } maptool;
    } data;
    u8 field_0x1a;
    u8 switch_no;
};
#pragma pack(pop)

class dEvt_control_c {
public:
    static dStage_MapEvent_dt_c* searchMapEventData(u8, s32);
};

class dEvent_manager_c {
public:
    s16 getEventIdx(fopAc_ac_c*, u8);
    s16 getEventIdx(fopAc_ac_c*, const char*, u8);
    int getMyStaffId(const char*, fopAc_ac_c*, int);
};

dEvent_manager_c& dComIfGp_getEventManager();
camera_process_class* dComIfGp_getCamera(int);
int dComIfGp_getPlayerCameraID(int);
fopAc_ac_c* dComIfGp_getPlayer(int);
dStage_roomDt_c* dComIfGp_roomControl_getStatusRoomDt(int);
const char* dComIfGp_getStartStageName();
int dComIfGp_roomControl_getStayNo();
BOOL dComIfGs_isTmpBit(u16);
class dComIfG_play_c { public: static int getLayerNo(int); };

void dComIfGp_event_onEventFlag(u16);
void dComIfGp_event_reset();
void dComIfGp_event_setItemPartner(void*);
void dComIfGp_event_setItemPartnerId(fpc_ProcID);
int dComIfGp_evmng_getMyStaffId(
    const char*, fopAc_ac_c*, int);
int dComIfGp_evmng_getMyActIdx(
    int, const char* const*, int, int, int);
int dComIfGp_evmng_getIsAddvance(int);
void dComIfGp_evmng_cutEnd(int);
int dComIfGp_evmng_endCheck(s16);
int dComIfGp_evmng_endCheck(const char*);

u8 dComIfGs_getItem(int, bool);
u8 dComIfGs_getBombMax(u8);
u8 dComIfGs_getBombNum(int);
u8 dComIfGs_getEventReg(u16);
void dComIfGs_setEventReg(u16, u8);
void dComIfGs_onTbox(int);

inline void dComIfGd_setXluListBG() {}

inline void dComIfGp_particle_setPolyColor(
    u16, const dBgS_GndChk&, const cXyz*, const dKy_tevstr_c*,
    const csXyz*, const cXyz*, int, const void*, int, const void*) {}

inline f32 cM_ssin(s16 angle) {
    return std::sin(
        static_cast<f32>(angle) * 3.14159265358979323846f / 32768.0f);
}
inline f32 cM_scos(s16 angle) {
    return std::cos(
        static_cast<f32>(angle) * 3.14159265358979323846f / 32768.0f);
}
inline f32 cM_s2rad(s16 angle) {
    return static_cast<f32>(angle) * 3.14159265358979323846f / 32768.0f;
}
inline s16 cM_deg2s(f32 degrees) {
    return static_cast<s16>(degrees * 32768.0f / 180.0f);
}

void cLib_offsetPos(cXyz*, const cXyz*, s16, const cXyz*);
void cLib_addCalc0(f32*, f32, f32);
void cLib_addCalcAngleS(s16*, s16, int, s16, s16);

constexpr int Z2SE_EN_PO_SOUL = 0x70000;
constexpr int Z2SE_OBJ_TBOX_OPEN_A = 0x801FA;
constexpr int Z2SE_OBJ_TBOX_OPEN_B = 0x801FB;
constexpr int Z2SE_OBJ_TBOX_OPEN_C = 0x801FC;
constexpr int Z2SE_OBJ_TBOX_OPEN_B_SLOW = 0x801FE;
constexpr int Z2SE_OBJ_T_BOX_EMERGE = 0x801FD;
constexpr int JA_SE_OBJ_BLOCK_FALL_NORMAL = 0x8002F;

#ifndef ARRAY_SIZEU
#define ARRAY_SIZEU(array) \
    (static_cast<unsigned int>(sizeof(array) / sizeof((array)[0])))
#endif
#ifndef OS_REPORT
#define OS_REPORT(...) ((void)0)
#endif
#ifndef OS_REPORT_ERROR
#define OS_REPORT_ERROR(...) ((void)0)
#endif
#ifndef OSReport_Error
#define OSReport_Error(...) ((void)0)
#endif

inline f32 JMAFastSqrt(f32 value) {
    return std::sqrt(value);
}

#endif
