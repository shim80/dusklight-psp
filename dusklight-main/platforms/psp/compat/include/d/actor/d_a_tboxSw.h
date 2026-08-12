#ifndef DUSK_PSP_COMPAT_D_A_TBOXSW_H
#define DUSK_PSP_COMPAT_D_A_TBOXSW_H

#include "f_op/f_op_actor_mng.h"

class daTboxSw_c : public fopAc_ac_c {
public:
    int Create();
    int create();
    int execute();
    int draw();
    int _delete();
};

namespace daTboxSw_prm {
inline u8 getTboxNo(daTboxSw_c* actor) {
    return fopAcM_GetParam(actor) & 0x3F;
}

inline u8 getSwNo(daTboxSw_c* actor) {
    return fopAcM_GetParam(actor) >> 8;
}
}  // namespace daTboxSw_prm

#endif
