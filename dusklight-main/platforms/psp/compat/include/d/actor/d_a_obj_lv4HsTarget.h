#ifndef DUSK_PSP_COMPAT_D_A_OBJ_LV4HSTARGET_H
#define DUSK_PSP_COMPAT_D_A_OBJ_LV4HSTARGET_H

#include "d/d_bg_s_movebg_actor.h"
#include "d/d_com_inf_game.h"
#include "m_Do/m_Do_mtx.h"
#include "m_Do/m_Do_hostIO.h"

class daLv4HsTarget_c : public dBgS_MoveBgActor {
public:
    void setBaseMtx();
    int create();
    int CreateHeap() override;
    int Execute(Mtx**) override;
    int Draw() override;
    int Delete() override;

    request_of_phase_process_class mPhase;
    J3DModel* mpModel;
    dKy_tevstr_c tevStr;
};

class daLv4HsTarget_HIO_c : public mDoHIO_entry_c {
public:
    daLv4HsTarget_HIO_c();
    ~daLv4HsTarget_HIO_c() override = default;
    void genMessage(JORMContext*);
};

#endif
