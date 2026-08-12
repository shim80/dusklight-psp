#ifndef DUSK_PSP_COMPAT_M_DO_MTX_H
#define DUSK_PSP_COMPAT_M_DO_MTX_H

// Source reference: include/m_Do/m_Do_mtx.h.

#include "f_op/f_op_actor_mng.h"

class mDoMtx_stack_c {
public:
    static MtxP now;
    static void transS(f32 x, f32 y, f32 z);
    static void transS(const cXyz& position) {
        transS(position.x, position.y, position.z);
    }
    static void YrotM(s16 angle);
    static void YrotS(s16 angle);
    static void ZXYrotM(s16 x, s16 y, s16 z);
    static void ZXYrotM(const csXyz& angle) {
        ZXYrotM(angle.x, angle.y, angle.z);
    }
    static void transM(f32 x, f32 y, f32 z);
    static void concat(MtxP matrix);
    static void multVec(const cXyz* source, cXyz* destination);
    static MtxP get();
};

void mDoMtx_inverse(MtxP source, Mtx destination);
void mDoMtx_multVec(MtxP matrix, const cXyz* source, cXyz* destination);
void mDoMtx_identity(Mtx destination);
void mDoMtx_copy(MtxP source, Mtx destination);
void mDoMtx_concat(MtxP left, MtxP right, Mtx destination);
void MTXRotAxisRad(Mtx destination, const cXyz* axis, f32 radians);

inline void MTXCopy(MtxP source, Mtx destination) {
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 4; ++column) {
            destination[row][column] = source[row][column];
        }
    }
}

inline void MTXTrans(MtxP destination, f32 x, f32 y, f32 z) {
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 4; ++column) {
            destination[row][column] = 0.0f;
        }
    }
    destination[0][0] = 1.0f;
    destination[1][1] = 1.0f;
    destination[2][2] = 1.0f;
    destination[0][3] = x;
    destination[1][3] = y;
    destination[2][3] = z;
}

#endif
