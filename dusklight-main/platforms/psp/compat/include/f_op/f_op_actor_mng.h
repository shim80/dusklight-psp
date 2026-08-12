#ifndef DUSK_PSP_COMPAT_F_OP_ACTOR_MNG_H
#define DUSK_PSP_COMPAT_F_OP_ACTOR_MNG_H

// Source references: include/f_op/f_op_actor*.h and include/f_pc/*.h in the
// repository snapshot. This is source compatibility, not GameCube ABI.

#include "d/dolzel_rel.h"

#include <new>

struct cXyz {
    f32 x;
    f32 y;
    f32 z;

    constexpr cXyz() = default;
    constexpr cXyz(f32 wanted_x, f32 wanted_y, f32 wanted_z)
        : x(wanted_x), y(wanted_y), z(wanted_z) {}

    void set(f32 wanted_x, f32 wanted_y, f32 wanted_z) {
        x = wanted_x;
        y = wanted_y;
        z = wanted_z;
    }

    f32 absXZ(const cXyz& other) const {
        const f32 dx = x - other.x;
        const f32 dz = z - other.z;
        return std::sqrt(dx * dx + dz * dz);
    }

    f32 abs2XZ() const { return x * x + z * z; }

    f32 abs(const cXyz& other) const {
        const f32 dx = x - other.x;
        const f32 dy = y - other.y;
        const f32 dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    cXyz& operator+=(const cXyz& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    cXyz& operator-=(const cXyz& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    static const cXyz BaseX;
    static const cXyz BaseY;
};

inline cXyz operator-(cXyz left, const cXyz& right) {
    left -= right;
    return left;
}

inline cXyz operator+(cXyz left, const cXyz& right) {
    left += right;
    return left;
}

using Vec = cXyz;

struct csXyz {
    s16 x;
    s16 y;
    s16 z;

    void set(s16 wanted_x, s16 wanted_y, s16 wanted_z) {
        x = wanted_x;
        y = wanted_y;
        z = wanted_z;
    }
};

using Mtx = f32[3][4];
using MtxP = f32 (*)[4];
using process_method_func = int (*)(void*);
class fopAc_ac_c;
using heapCallbackFunc = int (*)(fopAc_ac_c*);
using fpcLyIt_JudgeFunc = void* (*)(void*, void*);
using fopAcIt_JudgeFunc = fpcLyIt_JudgeFunc;

void mDoMtx_multVec(MtxP matrix, const cXyz* source, cXyz* destination);

struct process_method_class {
    process_method_func create_method;
    process_method_func delete_method;
    process_method_func execute_method;
    process_method_func is_delete_method;
};

struct leafdraw_method_class {
    process_method_class base;
    process_method_func draw_method;
};

struct actor_method_class {
    leafdraw_method_class base;
    u8 field_0x14[12];
};

struct process_profile_definition {
    u32 layer_id;
    u16 list_id;
    u16 list_priority;
    s16 name;
    const process_method_class* methods;
    u32 process_size;
    u32 unk_size;
    u32 parameters;
};

struct leaf_process_profile_definition {
    process_profile_definition base;
    const leafdraw_method_class* sub_method;
    s16 priority;
};

struct actor_process_profile_definition {
    leaf_process_profile_definition base;
    const actor_method_class* sub_method;
    u32 status;
    u8 group;
    u8 cullType;
};

struct actor_process_profile_definition2 {
    actor_process_profile_definition base;
    u32 field_0x30;
};

struct actor_place {
    cXyz pos;
    csXyz angle;
    s8 roomNo;
    u8 field_0x13;
};

class dEvt_info_c {
public:
    BOOL checkCommandDoor() const { return command_ == 3; }
    BOOL checkCommandDemoAccrpt() const { return command_ == 2; }
    void onCondition(u16 condition) { condition_ |= condition; }
    void setEventName(const char* name) { event_name_ = name; }
    void setEventId(s16 event_id) { event_id_ = event_id; }
    void setArchiveName(const char* archive) { archive_name_ = archive; }
    s16 getEventId() const { return event_id_; }
    u16 condition() const { return condition_; }
    const char* eventName() const { return event_name_; }
    void setCommand(u16 command) { command_ = command; }
    void clearCondition() { condition_ = 0; }

private:
    u16 command_ = 0;
    u16 condition_ = 0;
    s16 event_id_ = -1;
    const char* event_name_ = nullptr;
    const char* archive_name_ = nullptr;
};

struct attention_info_class {
    u32 flags = 0;
    cXyz position = {};
};

struct dKy_tevstr_c {
    u32 color = 0;
};

enum : u32 {
    fopAc_AttnFlag_JUEL_e = 1u << 0,
    fopAc_AttnFlag_UNK_0x400000 = 1u << 22,
};

class fopAc_ac_c {
public:
    fopAc_ac_c() {}
    virtual ~fopAc_ac_c() = default;

    actor_place home;
    actor_place old;
    actor_place current;
    csXyz shape_angle;
    cXyz scale;
    u32 parameters;
    s16 process_id;
    u8 actor_condition;
    MtxP actor_matrix;
    cXyz cull_min;
    cXyz cull_max;
    f32 speedF;
    cXyz speed;
    f32 gravity;
    cXyz eyePos;
    dEvt_info_c eventInfo;
    attention_info_class attention_info;
    dKy_tevstr_c tevStr;
};

enum : u32 {
    fpcLy_CURRENT_e = 0xFFFFFFFDu,
};

enum : u16 {
    fpcPi_CURRENT_e = 0xFFFDu,
};

enum : s16 {
    fpcNm_SCENE_EXIT_e = 0x030C,
};

enum {
    fpcDwPi_SCENE_EXIT_e = 761,
    fopAcStts_NOPAUSE_e = 1 << 17,
    fopAcStts_UNK_0x40000_e = 1 << 18,
    fopAcStts_UNK_0x4000_e = 1 << 14,
    fopAc_UNK_GROUP_5_e = 5,
    fopAc_CULLBOX_0_e = 0,
    fopAcCnd_INIT_e = 0x08,
    fopAcStts_CULL_e = 1 << 8,
    fopAc_ACTOR_e = 0,
    fopAc_CULLBOX_CUSTOM_e = 14,
};

extern const leafdraw_method_class g_fpcLf_Method;
extern actor_method_class g_fopAc_Method;

inline u32 fopAcM_GetParam(const void* actor) {
    return static_cast<const fopAc_ac_c*>(actor)->parameters;
}

inline u32 fopAcM_GetParamBit(
    const void* actor, unsigned int shift, unsigned int bits) {
    const u32 mask = bits >= 32 ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    return (fopAcM_GetParam(actor) >> shift) & mask;
}

inline s8 fopAcM_GetHomeRoomNo(const fopAc_ac_c* actor) {
    return actor->home.roomNo;
}

inline s8 fopAcM_GetRoomNo(const fopAc_ac_c* actor) {
    return actor->current.roomNo;
}

inline void fopAcM_SetSpeedF(fopAc_ac_c* actor, f32 speed) {
    actor->speedF = speed;
}

inline cXyz* fopAcM_GetPosition_p(fopAc_ac_c* actor) {
    return &actor->current.pos;
}

inline cXyz* fopAcM_GetOldPosition_p(fopAc_ac_c* actor) {
    return &actor->old.pos;
}

inline cXyz* fopAcM_GetSpeed_p(fopAc_ac_c* actor) {
    return &actor->speed;
}

inline f32 fopAcM_GetGravity(const fopAc_ac_c* actor) {
    return actor->gravity;
}

inline void fopAcM_SetGravity(fopAc_ac_c* actor, f32 gravity) {
    actor->gravity = gravity;
}

BOOL dComIfGs_isSwitch(int number, int room);
void dComIfGs_onSwitch(int number, int room);
void dComIfGs_offSwitch(int number, int room);

inline BOOL fopAcM_isSwitch(const fopAc_ac_c* actor, int number) {
    return dComIfGs_isSwitch(number, fopAcM_GetHomeRoomNo(actor));
}

inline void fopAcM_onSwitch(const fopAc_ac_c* actor, int number) {
    dComIfGs_onSwitch(number, fopAcM_GetHomeRoomNo(actor));
}

inline void fopAcM_offSwitch(const fopAc_ac_c* actor, int number) {
    dComIfGs_offSwitch(number, fopAcM_GetHomeRoomNo(actor));
}

#define fopAcM_ct(ptr, ClassName)                                      \
    do {                                                               \
        if ((ptr)->actor_condition != fopAcCnd_INIT_e) {               \
            new (ptr) ClassName;                                       \
            (ptr)->actor_condition = fopAcCnd_INIT_e;                  \
        }                                                              \
    } while (false)

inline void fopAcM_SetMtx(fopAc_ac_c* actor, MtxP matrix) {
    actor->actor_matrix = matrix;
}

inline void fopAcM_setCullSizeBox(
    fopAc_ac_c* actor, f32 min_x, f32 min_y, f32 min_z,
    f32 max_x, f32 max_y, f32 max_z) {
    actor->cull_min = {min_x, min_y, min_z};
    actor->cull_max = {max_x, max_y, max_z};
}

class J3DModelData;
void dusk_psp_compat_set_cull_box(
    fopAc_ac_c* actor, const J3DModelData* model_data);

inline void fopAcM_setCullSizeBox2(
    fopAc_ac_c* actor, const J3DModelData* model_data) {
    dusk_psp_compat_set_cull_box(actor, model_data);
}

int fopAcM_entrySolidHeap(
    fopAc_ac_c* actor, heapCallbackFunc callback, u32 size);
void* fpcM_Search(fpcLyIt_JudgeFunc judge, void* data);
#define fopAcM_Search fpcM_Search
void* fopAcM_SearchByID(fpc_ProcID id);
fpc_ProcID fopAcM_GetID(const void* actor);
s16 fpcM_GetProfName(const void* actor);
BOOL fopAc_IsActor(const void* actor);
void fopAcM_seStartLevel(fopAc_ac_c* actor, int sound_id, s16 speed);
inline void fopAcM_seStart(fopAc_ac_c*, int, int) {}
int fopAcM_delete(fopAc_ac_c* actor);
fpc_ProcID fopAcM_createItemForPresentDemo(
    const cXyz*, u8 item, int, int, int, const void*, const void*);
fpc_ProcID fopAcM_createItemForTrBoxDemo(
    const cXyz*, u8 item, int, int, const void*, const void*);
int fopAcM_orderOtherEvent(
    fopAc_ac_c*, const char*, u16, u16, u8);
int fopAcM_orderOtherEventId(
    fopAc_ac_c*, s16, u8, u16, u8, u8);
int fopAcM_seenPlayerAngleY(const fopAc_ac_c*);
int fopAcM_seenActorAngleY(
    const fopAc_ac_c*, const fopAc_ac_c*);
bool fopAcM_myRoomSearchEnemy(int room);
void fopAcM_posMoveF(fopAc_ac_c*, const cXyz*);
inline void fopAcM_setWarningMessage(...) {}
inline void fopAcM_setEffectMtx(fopAc_ac_c*, J3DModelData*) {}

class fopAcM_lc_c {
public:
    static bool lineCheck(
        const cXyz*, const cXyz*, const fopAc_ac_c*);
    static bool checkMoveBG();
    static cXyz* getCrossP();
};

class fopAcM_gc_c {
public:
    static bool gndCheck(const cXyz*);
    static f32 getGroundY();
};

#endif
