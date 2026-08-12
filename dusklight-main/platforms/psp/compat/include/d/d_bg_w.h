#ifndef DUSK_PSP_COMPAT_D_BG_W_H
#define DUSK_PSP_COMPAT_D_BG_W_H

#include "dusk/psp/movebg_runtime.hpp"
#include "f_op/f_op_actor_mng.h"

namespace dusk::psp::model {
class PspStaticModelRuntime;
}

struct cBgD_t {};

class dBgW {
public:
    int Set(cBgD_t* collision, u32 flags, Mtx* matrix);
    void Move();
    void SetCrrFunc(void (*)()) {}
    bool ChkUsed() const { return used_; }

private:
    friend class dBgS_CompatWorld;
    friend class dusk::psp::model::PspStaticModelRuntime;
    const void* collision_ = nullptr;
    std::uint32_t collision_size_ = 0;
    Mtx* matrix_ = nullptr;
    dusk::psp::movebg::Handle handle_ = {};
    bool configured_ = false;
    bool used_ = false;
};

#endif
