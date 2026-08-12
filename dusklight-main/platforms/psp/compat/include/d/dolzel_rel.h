#ifndef DUSK_PSP_COMPAT_D_DOLZEL_REL_H
#define DUSK_PSP_COMPAT_D_DOLZEL_REL_H

// Source reference: include/d/dolzel_rel.h in the repository snapshot
// recorded by docs/decisions/0004-dusklight-snapshot-provenance.md.

#include <cmath>
#include <cstddef>
#include <cstdint>

using u8 = std::uint8_t;
using s8 = std::int8_t;
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using f32 = float;
using BOOL = int;
using fpc_ProcID = std::uint32_t;
using cPhs_Step = int;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define DUSK_CONST const
#define DUSK_PROFILE extern
#define DUSK_GAME_DATA
#define DUSK_GAME_EXTERN extern
#define DUSK_CONSTEXPR constexpr
#define STATIC_ASSERT(condition)

struct DuskPspActorHeapNewTag {};
void* operator new(std::size_t size, DuskPspActorHeapNewTag) noexcept;
#define JKR_NEW new (DuskPspActorHeapNewTag{})

#define JUT_ASSERT(line, condition) ((void)(condition))

enum : fpc_ProcID {
    fpcM_ERROR_PROCESS_ID_e = 0xFFFFFFFFu,
};

enum : int {
    cPhs_INIT_e = 0,
    cPhs_LOADING_e = 1,
    cPhs_NEXT_e = 2,
    cPhs_UNK3_e = 3,
    cPhs_COMPLEATE_e = 4,
    cPhs_ERROR_e = 5,
};

struct request_of_phase_process_class {
    u32 state;
    u32 generation;
};
constexpr int Z2SE_FORCE_BACK = 0;
constexpr int Z2SE_WL_V_FALL_TO_RESTART = 0;
constexpr int Z2SE_OBJ_SPNR_GEAR_S = 0x8019F;
constexpr int Z2SE_OBJ_SPNR_GEAR_L = 0x801C1;
constexpr int Z2SE_READ_RIDDLE_A = 0x0000C;
constexpr int Z2SE_OBJ_POU_GATE_CL = 0x801B3;
constexpr int Z2SE_OBJ_POU_GATE_OP = 0x801B4;
constexpr int Z2SE_OBJ_SPNR_SW_CL = 0x801B5;
constexpr int Z2SE_OBJ_SPNR_SW_FIT = 0x801B6;
constexpr int Z2SE_OBJ_SPNR_SW_RL = 0x801B7;

inline void mDoAud_seStart(int, const void*, int, int) {}

inline int cLib_chaseS(s16* value, s16 target, s16 step) {
    if (step != 0) {
        if (*value > target) {
            step = static_cast<s16>(-step);
        }
        *value = static_cast<s16>(*value + step);
        if (step * (*value - target) >= 0) {
            *value = target;
            return 1;
        }
    } else if (*value == target) {
        return 1;
    }
    return 0;
}

inline f32 cLib_addCalc(
    f32* value, f32 target, f32 scale, f32 maximum_step,
    f32 minimum_step) {
    f32 step = 0.0f;
    if (*value != target) {
        step = scale * (target - *value);
        if (step >= minimum_step || step <= -minimum_step) {
            if (step > maximum_step) {
                step = maximum_step;
            }
            if (step < -maximum_step) {
                step = -maximum_step;
            }
            *value += step;
        } else if (step > 0.0f) {
            *value += minimum_step;
            if (*value > target) {
                *value = target;
            }
        } else {
            *value -= minimum_step;
            if (*value < target) {
                *value = target;
            }
        }
    }
    return std::fabs(target - *value);
}

inline s16 cLib_distanceAngleS(s16 target, s16 current) {
    return static_cast<s16>(target - current);
}


inline f32 cLib_addCalc2(f32* value, f32 target, f32 scale, f32 max_step) {
    return cLib_addCalc(value, target, scale, max_step, 0.0001f);
}
inline int cLib_chaseUC(u8* value, u8 target, u8 step) {
    if (*value == target) return 1;
    if (*value > target) {
        const u8 delta = static_cast<u8>(*value - target);
        *value = delta <= step ? target : static_cast<u8>(*value - step);
    } else {
        const u8 delta = static_cast<u8>(target - *value);
        *value = delta <= step ? target : static_cast<u8>(*value + step);
    }
    return *value == target;
}

inline int cLib_chaseF(f32* value, f32 target, f32 step) {
    if (step != 0.0f) {
        if (*value > target) {
            step = -step;
        }
        *value += step;
        if (step * (*value - target) >= 0.0f) {
            *value = target;
            return 1;
        }
    } else if (*value == target) {
        return 1;
    }
    return 0;
}

#endif
