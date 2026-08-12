#ifndef DUSK_PSP_COMPAT_D_A_OBJ_SWSPINNER_H
#define DUSK_PSP_COMPAT_D_A_OBJ_SWSPINNER_H

#include "d/actor/d_a_spinner.h"
#include "d/d_bg_s_movebg_actor.h"
#include "d/d_bg_w.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_mtx.h"

// Source-compatible layout for the unchanged original implementation. It is a
// native PSP layout and intentionally does not claim GameCube ABI parity.
class daObjSwSpinner_c : public dBgS_MoveBgActor {
public:
    void initBaseMtx();
    void setBaseMtx();
    int Create() override;
    int CreateHeap() override;
    int create1st();
    int Execute(Mtx**) override;
    int Draw() override;
    int Delete() override;

    u8 getSwbit() {
        return static_cast<u8>(fopAcM_GetParamBit(this, 0, 8));
    }
    u8 getSwbit2() {
        return static_cast<u8>(fopAcM_GetParamBit(this, 8, 8));
    }
    int GetRotSpeedY() { return mRotSpeedY; }
    f32 GetR() { return 100.0f; }

    const J3DModel* duskPspBaseModel() const { return mpModelA; }
    const J3DModel* duskPspTopModel() const { return mpModelB; }
    s16 duskPspRotationSpeed() const { return mRotSpeedY; }
    s16 duskPspRotationCount() const { return mCount; }
    bool duskPspSpinnerIn() const { return mSpinnerIn; }
    bool duskPspCanUse() const { return mCanUse; }
    void duskPspSetRotSpeedY(s16 speed) { mRotSpeedY = speed; }

    request_of_phase_process_class mPhase;
    J3DModel* mpModelA;
    J3DModel* mpModelB;
    dBgW* mpBgW2;
    Mtx mMtx;
    f32 mPartBHeight;
    bool mSpinnerIn;
    bool mPrevSpinnerIn;
    bool mCanUse;
    bool mPrevIsSwbit2;
    s16 mPrevAngle;
    s16 mRotSpeedY;
    s16 mCount;
    struct {
        cXyz position;
    } attention_info;
    dKy_tevstr_c tevStr;
};

#endif
