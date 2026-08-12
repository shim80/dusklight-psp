#ifndef DUSK_PSP_COMPAT_D_BG_S_MOVEBG_ACTOR_H
#define DUSK_PSP_COMPAT_D_BG_S_MOVEBG_ACTOR_H

#include "f_op/f_op_actor_mng.h"
#include "m_Do/m_Do_ext.h"

using dBgS_MoveBGProc = void (*)();
void dBgS_MoveBGProc_TypicalRotY();
class dBgW;

class dBgS_MoveBgActor : public fopAc_ac_c {
public:
    virtual int Create() { return 1; }
    virtual int CreateHeap() = 0;
    virtual int Execute(Mtx**) = 0;
    virtual int Draw() = 0;
    virtual int Delete() = 0;

    int MoveBGCreate(
        const char* archive, int collision_resource,
        dBgS_MoveBGProc transform, std::uint32_t heap_size,
        MtxP matrix);
    int MoveBGExecute();
    int MoveBGDraw();
    int MoveBGDelete();

    Mtx mBgMtx = {};
    dBgW* mpBgW = nullptr;
};

#endif
