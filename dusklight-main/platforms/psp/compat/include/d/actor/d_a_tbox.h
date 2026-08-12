#ifndef DUSK_PSP_COMPAT_D_A_TBOX_H
#define DUSK_PSP_COMPAT_D_A_TBOX_H

// Native PSP declaration of the real snapshot class. Method names and fields
// are those consumed by src/d/actor/d_a_tbox.cpp; GameCube offsets are not
// preserved or claimed.

#include "d/actor/d_a_tbox_support.h"
#include "d/d_bg_s_movebg_actor.h"
#include "d/d_bg_w.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_hostIO.h"

class daTboxBase_c : public dBgS_MoveBgActor {
protected:
    virtual BOOL checkSmallTbox() = 0;
    dCcD_Stts mStts;
    dCcD_Cyl mCyl;
};

struct daTbox_ModelInfo {
    DUSK_CONST char* mArcName;
    int mModelResNo;
    int mBckResNo;
    int mClosedDzbResNo;
    int mOpenDzbResNo;
    int mEffectResNo;
    int mBrkResNo;
};

class daTbox_c;
using daTbox_actionFn = int (daTbox_c::*)();
using daTbox_modeFn = void (daTbox_c::*)();

class daTbox_c : public daTboxBase_c {
public:
    enum Shape {
        SHAPE_SMALL = 0,
        SHAPE_LARGE = 1,
        SHAPE_BOSSKEY = 2,
    };
    enum Mode {
        MODE_EXEC_WAIT = 0,
        MODE_EXEC = 1,
    };

    DUSK_CONST daTbox_ModelInfo* getModelInfo();
    int commonShapeSet();
    int effectShapeSet();
    int envShapeSet();
    int bgCheckSet();
    void lightReady();
    void setLightPos();
    int checkEnv();
    int checkAppear();
    int checkOpen();
    void clrDzb();
    void setDzb();
    void surfaceProc();
    int checkNormal();
    int checkEnvEffectTbox();
    u32 calcHeapSize();
    int CreateHeap() override;
    void CreateInit();
    void initPos();
    void initAnm();
    int boxCheck();
    void demoProcOpen();
    void lightColorProc();
    void environmentProc();
    void lightUpProc();
    void lightDownProc();
    void dropProcInitCall();
    void dropProcInit();
    int calcJumpGoalAndAngle(cXyz*, s16*);
    bool getDropSAngle(s16*);
    int getDir();
    void setRotAxis(cXyz const*, cXyz const*);
    void dropProcInit2();
    void dropProc();
    void demoInitAppear();
    void demoProcAppear();
    int demoProc();
    void OpenInit_com();
    void OpenInit();
    int actionWait();
    int actionDemo();
    int actionDemo2();
    int actionDropDemo();
    u8 getBombItemNo(u8, u8);
    u8 getBombItemNo2(u8, u8, u8);
    u8 getBombItemNo3(u8, u8, u8, u8);
    u8 getBombItemNoMain(u8);
    int setGetDemoItem();
    int actionOpenWait();
    int actionNotOpenDemo();
    int checkDrop();
    void settingDropDemoCamera();
    int actionSwOnWait();
    int actionSwOnWait2();
    int actionDropWait();
    int actionGenocide();
    int actionDropWaitForWeb();
    int actionDropForWeb();
    void initBaseMtx();
    void setBaseMtx();
    void mode_proc_call();
    void mode_exec_wait();
    void mode_exec();
    int create1st();
    int Execute(Mtx**) override;
    int Draw() override;
    int Delete() override;
    int Create() override { return true; }

    BOOL checkSmallTbox() override { return TRUE; }

    u32 getEvent() { return fopAcM_GetParam(this) >> 24; }
    int getShapeType() { return (fopAcM_GetParam(this) >> 20) & 0xf; }
    int getSwNo() { return (fopAcM_GetParam(this) >> 12) & 0xff; }
    int getTboxNo() { return (fopAcM_GetParam(this) >> 6) & 0x3f; }
    int getFuncType() { return fopAcM_GetParam(this) & 0x3f; }
    u32 getSwType() { return field_0x980 & 0xf; }
    int getItemNo() { return (field_0x982 >> 8) & 0xff; }
    int getPathId() { return field_0x982 & 0xff; }
    void flagClr() { mFlags = 0; }
    void flagOn(u16 flag) { mFlags |= flag; }
    void flagOff(u16 flag) { mFlags &= ~flag; }
    u16 flagCheck(u16 flag) { return mFlags & flag; }
    void setAction(daTbox_actionFn action) { mpActionFn = action; }
    void action() { (this->*mpActionFn)(); }
    void setDrawMtx(Mtx matrix) {
        MTXCopy(matrix, mDrawMtx);
        field_0x9fc = 1;
    }

private:
    u8 field_0x718 = 0;
    request_of_phase_process_class mPhase = {};
    J3DModel* mpModel = nullptr;
    J3DModel* mpSlimeModel = nullptr;
    mDoExt_bckAnm* mpAnm = nullptr;
    J3DModel* mpEffectModel = nullptr;
    mDoExt_brkAnm* mpEffectAnm = nullptr;
    dBgW* mpOpenBgW = nullptr;
    dBgW* mpBgCollision = nullptr;
    daTbox_actionFn mpActionFn = nullptr;
    int mStaffId = -1;
    f32 field_0x750 = 0.0f;
    u16 mFlags = 0;
    u16 mDemoFrame = 0;
    bool field_0x758 = false;
    u8 field_0x759 = 0;
    u16 field_0x75a = 0;
    u8 mTimer = 0;
    dBgS_ObjAcch mAcch;
    dBgS_AcchCir mAcchCir;
    u8 mTboxNo = 0;
    s16 mEventId = -1;
    bool field_0x97c = false;
    bool field_0x97d = false;
    u8 field_0x97e = 0;
    bool mParamsInit = false;
    u16 field_0x980 = 0;
    u16 field_0x982 = 0;
    s16 field_0x984 = -1;
    Mtx field_0x988 = {};
    cXyz mRotAxis = {};
    s16 field_0x9c4 = 0;
    s16 field_0x9c6 = 0;
    u8 field_0x9c8 = 0;
    u8 field_0x9c9 = 0;
    s16 field_0x9ca = 0;
    u8 field_0x9cc = 0;
    u8 mMode = 0;
    LIGHT_INFLUENCE mLight = {};
    f32 mAllcolRatio = 1.0f;
    int field_0x9f4 = 0;
    u32 mOpenSeId = 0;
    u8 field_0x9fc = 0;
    u8 field_0x9fd = 0;
    Mtx mDrawMtx = {};
};

#endif
