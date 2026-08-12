#ifndef DUSK_PSP_COMPAT_D_COM_INF_GAME_H
#define DUSK_PSP_COMPAT_D_COM_INF_GAME_H

// Source references: include/d/d_com_inf_game.h and include/d/d_save.h.

#include "f_op/f_op_actor_mng.h"

class dSv_event_flag_c {
public:
    static const u16 saveBitLabels[822];
};

BOOL dComIfGs_isEventBit(u16 flag);
BOOL dComIfGs_isTbox(int number);

int dComIfG_resLoad(
    request_of_phase_process_class* phase, const char* archive);
int dComIfG_resDelete(
    request_of_phase_process_class* phase, const char* archive);
void* dComIfG_getObjectRes(const char* archive, int resource_id);

class dBgW;
struct dBgS_GndChk;
class dBgS_CompatWorld {
public:
    int Regist(dBgW* collision, fopAc_ac_c* owner);
    int Release(dBgW* collision);
    f32 GroundCross(dBgS_GndChk*) { return 0.0f; }
    int GetMtrlSndId(const dBgS_GndChk&) { return 0; }
};
dBgS_CompatWorld& dComIfG_Bgsp();

class J3DModel;
class dScnKy_env_light_c {
public:
    void settingTevStruct(int, const cXyz*, dKy_tevstr_c*);
    void setLightTevColorType_MAJI(J3DModel*, dKy_tevstr_c*);
};

extern dScnKy_env_light_c g_env_light;

class dVibration_c {
public:
    bool StartShock(int, int, cXyz) { return false; }
};

inline dVibration_c& dComIfGp_getVibration() {
    static dVibration_c vibration;
    return vibration;
}

inline s8 dComIfGp_getReverb(int) {
    return 0;
}

class JPABaseEmitter;
inline JPABaseEmitter* dComIfGp_particle_set(
    u16, const cXyz*, const csXyz*, const cXyz*, u8,
    void*, s8, const void*, const void*, const cXyz*) {
    return nullptr;
}

inline void dComIfGd_setListBG() {}
inline void dComIfGd_setList() {}

#endif
