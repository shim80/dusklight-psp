#ifndef DUSK_PSP_COMPAT_D_A_PLAYER_H
#define DUSK_PSP_COMPAT_D_A_PLAYER_H

// Source reference: include/d/actor/d_a_player.h.

#include "f_op/f_op_actor_mng.h"

class daMidna_c;

bool dusk_psp_compat_scene_change(
    u8 exit_index, u8 path_id, fopAc_ac_c* source, bool jump);
bool dusk_psp_compat_scene_change_started();

class daPy_py_c : public fopAc_ac_c {
public:
    static daMidna_c* getMidnaActor();
    static bool checkNowWolf() { return false; }
    bool checkTreasureRupeeReturn(u8) const { return false; }
    bool checkHorseRide() const { return false; }
    const cXyz* getLeftFootPosP() const { return &left_foot_pos_; }
    void duskPspSetLeftFootPos(const cXyz& pos) { left_foot_pos_ = pos; }
    float getBaseAnimeFrame() const { return mDuskPspBaseAnimeFrame; }
    void duskPspSetBaseAnimeFrame(float frame) { mDuskPspBaseAnimeFrame = frame; }
    const cXyz* getKandelaarFlamePos() const { return nullptr; }
    void onSceneChangeArea(u8 exit_index, u8 path_id, fopAc_ac_c* source) {
        dusk_psp_compat_scene_change(
            exit_index, path_id, source, false);
    }

    void onSceneChangeAreaJump(
        u8 exit_index, u8 path_id, fopAc_ac_c* source) {
        dusk_psp_compat_scene_change(
            exit_index, path_id, source, true);
    }

    BOOL checkSceneChangeAreaStart() const {
        return dusk_psp_compat_scene_change_started() ? TRUE : FALSE;
    }

    void voiceStart(int) {}

private:
    float mDuskPspBaseAnimeFrame = 0.0f;
    cXyz left_foot_pos_ = {};
};

daPy_py_c* daPy_getPlayerActorClass();

#endif
