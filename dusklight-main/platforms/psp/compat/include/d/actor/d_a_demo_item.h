#ifndef DUSK_PSP_COMPAT_D_A_DEMO_ITEM_H
#define DUSK_PSP_COMPAT_D_A_DEMO_ITEM_H

#include "d/actor/d_a_tbox_support.h"
#include "m_Do/m_Do_ext.h"

class JPABaseEmitter {
public:
    void setGlobalAlpha(u8) {}
    void setGlobalTranslation(const cXyz&) {}
    void stopCreateParticle() {}
    void quitImmortalEmitter() {}
    void becomeInvalidEmitter() {}
};

class dPa_followEcallBack {
public:
    JPABaseEmitter* getEmitter() { return emitter_; }
    const JPABaseEmitter* getEmitter() const { return emitter_; }
    void remove() { emitter_ = nullptr; }
    void setEmitter(JPABaseEmitter* emitter) { emitter_ = emitter; }
private:
    JPABaseEmitter* emitter_ = nullptr;
};

class Z2SoundObjSimple {
public:
    void init(const cXyz*, int) {}
    void startLevelSound(int, int, int) {}
    void framework(int, s8) {}
    void deleteObject() {}
};

struct daItemBase_data {
    f32 mGravity;
    f32 mGroundReflect;
    f32 mLaunchSpeed;
    f32 mScalingTime;
    f32 mSpeedH;
    s16 mFlashCycleTime;
    s16 mWaitTime;
    s16 mDisappearTime;
    s16 mRotateXSpeed;
    s16 mRotateYSpeed;
    f32 mHeartFallSpeed;
    f32 mHeartAmplitude;
    s16 mHeartFallCycleTime;
    s16 mHeartTilt;
    f32 mGetDemoLaunchSpeed;
    f32 mGetDemoGravity;
    s16 mSimpleExistTime;
    s16 mNoGetTime;
};

class daItemBase_c : public fopAc_ac_c {
public:
    u8 getItemNo();
    void hide();
    void show();
    void changeDraw();
    bool chkDraw();
    void dead();
    bool chkDead();
    int CreateItemHeap(const char*, s16, s16, s16, s16, s16, s16, s16);
    int DeleteBase(const char*);
    void setListEnd();
    void animPlay(f32, f32, f32, f32, f32, f32);
    const daItemBase_data& getData() { return m_data; }

    virtual int DrawBase();
    virtual void setListStart();
    virtual void settingBeforeDraw();
    virtual void setTevStr();
    virtual void setShadow();
    virtual void animEntry();
    virtual void RotateYBase();
    virtual int clothCreate();
    virtual int __CreateHeap();
    virtual BOOL chkFlag(int);
    virtual s8 getTevFrm();
    virtual s8 getBtpFrm();
    virtual u8 getShadowSize();
    virtual u8 getCollisionH();
    virtual u8 getCollisionR();

    static const daItemBase_data m_data;

    request_of_phase_process_class mPhase = {};
    J3DModel* mpModel = nullptr;
    void* mpBtkAnm = nullptr;
    void* mpBpkAnm = nullptr;
    mDoExt_brkAnm* mpBrkAnm = nullptr;
    mDoExt_bckAnm* mpBckAnm = nullptr;
    void* mpBtpAnm = nullptr;
    u32 mShadowKey = 0;
    u32 mItemBitNo = 0;
    int m_timer = 0;
    s16 m_get_timer = 0;
    u8 m_itemNo = dItemNo_NONE_e;
    u8 field_0x92b = 0;
};

class daDitem_c : public daItemBase_c {
public:
    enum Action_e { ACTION_START_e, ACTION_EVENT_e, ACTION_WAIT_LIGHT_END_e, ACTION_END_e };

    int CreateInit();
    void action();
    void actionStart();
    void actionEvent();
    void actionWaitLightEnd();
    void actionEnd();
    void setInsectEffect();
    void followInsectEffect();
    void endInsectEffect();
    void onEventReg(int, int);
    void set_pos();
    void anim_control();
    void initEffectLight();
    void settingEffectLight();
    void set_mtx();
    void draw_WOOD_STICK();
    virtual void setListStart();
    virtual void setTevStr();
    virtual int __CreateHeap();
    int Delete();
    int create();
    int execute();
    int draw();
    void setAction(u8 action) { mAction = action; }
    u8 chkArgFlag(u8 flag) { return field_0x93d & flag; }
    void setOffsetPos(cXyz pos) { mOffsetPos = pos; }
    void setMaxScale(f32 value) { mMaxScale = value; }

    cXyz mOffsetPos = {};
    f32 mMaxScale = 0.0f;
    u8 field_0x93c = 0;
    u8 field_0x93d = 0;
    u8 field_0x93e = 0;
    u8 mSetLightEff = 0;
    f32 mLightStrength = 0.0f;
    LIGHT_INFLUENCE mLight = {};
    u8 mAction = ACTION_START_e;
    u8 field_0x969 = 0;
    u8 mParticleAlpha = 0;
    dPa_followEcallBack field_0x96c;
    dPa_followEcallBack field_0x980;
    JPABaseEmitter* field_0x994 = nullptr;
    JPABaseEmitter* field_0x998 = nullptr;
    cXyz field_0x99c = {};
    Z2SoundObjSimple mSound;
};

namespace daDitem_prm {
inline u8 getFlag(daDitem_c* actor) { return (fopAcM_GetParam(actor) >> 16) & 0xFF; }
inline u8 getNo(daDitem_c* actor) { return fopAcM_GetParam(actor) & 0xFF; }
}

int CheckItemCreateHeap(fopAc_ac_c* actor);

#endif
