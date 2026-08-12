#ifndef DUSK_PSP_COMPAT_D_A_OBJ_LV4GEAR_H
#define DUSK_PSP_COMPAT_D_A_OBJ_LV4GEAR_H

#include "d/actor/d_a_obj_swspinner.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_mtx.h"

class daObjLv4Gear_c : public fopAc_ac_c {
public:
    enum gear_type_e {
        GEAR_S_e,
        GEAR_L_e,
    };

    void initBaseMtx();
    void setBaseMtx();
    int Create();
    int CreateHeap();
    int create();
    int execute();
    int draw();
    int _delete();

    u8 getSwBit() {
        return static_cast<u8>(fopAcM_GetParamBit(this, 0, 8));
    }
    u8 getType() {
        return static_cast<u8>(fopAcM_GetParamBit(this, 8, 4));
    }
    u8 checkSE() {
        return static_cast<u8>(fopAcM_GetParamBit(this, 12, 4));
    }

    s16 duskPspSpeed() const {
        return mSpeed;
    }
    s16 duskPspRotation() const {
        return mRotation;
    }
    s16 duskPspTarget() const {
        return mTarget;
    }
    u16 duskPspCount() const {
        return mCount;
    }
    const J3DModel* duskPspModel() const {
        return mpModel;
    }

private:
    dKy_tevstr_c tevStr;
    request_of_phase_process_class mPhase;
    J3DModel* mpModel;
    s16 mTarget;
    s16 mSpeed;
    s16 mRotation;
    fpc_ProcID mSwActorID;
    u16 mCount;
    u8 mType;
};

#endif
