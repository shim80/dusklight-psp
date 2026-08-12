#ifndef DUSK_PSP_COMPAT_D_A_OBJ_LV4POGATE_H
#define DUSK_PSP_COMPAT_D_A_OBJ_LV4POGATE_H

#include "d/d_bg_s_movebg_actor.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_hostIO.h"
#include "m_Do/m_Do_mtx.h"

class daLv4PoGate_c : public dBgS_MoveBgActor {
public:
    enum Mode_e {
        MODE_WAIT_e,
        MODE_MOVE_OPEN_e,
        MODE_MOVE_CLOSE_e,
        MODE_MOVE_CLOSE_WAIT_e,
        MODE_MOVE_CLOSE2_e,
        MODE_MOVE_CLOSE2_WAIT_e,
        MODE_MOVE_CLOSE3_e,
    };

    void setBaseMtx();
    int create();
    void moveGate();
    void init_modeWait();
    void modeWait();
    void init_modeMoveOpen();
    void modeMoveOpen();
    void init_modeMoveClose();
    void modeMoveClose();
    void init_modeMoveCloseWait();
    void modeMoveCloseWait();
    void init_modeMoveClose2();
    void modeMoveClose2();
    void init_modeMoveClose2Wait();
    void modeMoveClose2Wait();
    void init_modeMoveClose3();
    void modeMoveClose3();
    void setSe();
    void setEffect(int);

    int CreateHeap() override;
    int Execute(Mtx**) override;
    int Draw() override;
    int Delete() override;

    int getSw() {
        return static_cast<int>(fopAcM_GetParamBit(this, 0, 8));
    }
    float duskPspMoveValue() const { return mMoveValue; }
    std::uint8_t duskPspMode() const { return mMode; }
    std::uint8_t duskPspSwitch() const { return mSw; }
    const J3DModel* duskPspModel() const { return mpModel; }

private:
    request_of_phase_process_class mPhase;
    J3DModel* mpModel;
    dKy_tevstr_c tevStr;
    u8 mMode;
    u8 mSw;
    u8 mInitMove;
    f32 mMoveTarget;
    f32 mMoveValue;
    u8 mCloseWaitTime;
};

class daLv4PoGate_HIO_c : public mDoHIO_entry_c {
public:
    daLv4PoGate_HIO_c();
    ~daLv4PoGate_HIO_c() override = default;

    void genMessage(JORMContext*);

    f32 mOpenSpeed;
    f32 mCloseStep1Speed;
    f32 mCloseStep2Speed;
    f32 mCloseStep1Amount;
    f32 mCloseStep2Amount;
    u8 mCloseStep1Wait;
    u8 mCloseStep2Wait;
    f32 mCloseStep3Speed;
    f32 mCloseStep3Max;
    u8 mShockStrength;
};

#endif
