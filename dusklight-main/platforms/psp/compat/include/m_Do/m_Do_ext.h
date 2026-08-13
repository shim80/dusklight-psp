#ifndef DUSK_PSP_COMPAT_M_DO_EXT_H
#define DUSK_PSP_COMPAT_M_DO_EXT_H

#include "f_op/f_op_actor_mng.h"
#include "dusk/psp/bck_runtime.hpp"

#include <cstdint>
#include <cstring>

namespace dusk::psp::model {
class PspStaticModelRuntime;
}

class J3DShape { public: void hide() {} };
class J3DMaterial { public: J3DShape* getShape() { static J3DShape shape; return &shape; } };
class JUTNameTab { public: const char* getName(std::uint16_t) const { return ""; } };
class J3DMaterialTable { public: JUTNameTab* getMaterialName() { static JUTNameTab tab; return &tab; } };
class J3DJoint {
public:
    const cXyz* getMax() const { return &max_; }
    void setMax(const cXyz& value) { max_ = value; }
private:
    cXyz max_ = {0.0f, 20.0f, 0.0f};
};

class J3DModelData {
public:
    static constexpr std::uint32_t kTevRegisterCapacity = 32;

    const void* model_bytes() const;
    J3DJoint* getJointNodePointer(std::uint16_t) { return &joint_stub_; }
    const J3DJoint* getJointNodePointer(std::uint16_t) const { return &joint_stub_; }
    J3DMaterialTable& getMaterialTable() { return material_table_; }
    std::uint16_t getMaterialNum() const { return 0; }
    J3DMaterial* getMaterialNodePointer(std::uint16_t) { return &material_stub_; }
    std::uint32_t model_size() const;
    const void* texture_bytes() const;
    std::uint32_t texture_size() const;
    void record_animation(
        std::uint32_t resource_id, float frame,
        std::uint32_t joints) {
        animation_resource_id_ = resource_id;
        animation_frame_ = frame;
        animation_joints_ = joints;
        ++animation_applications_;
    }
    std::uint32_t animation_resource_id() const {
        return animation_resource_id_;
    }
    float animation_frame() const { return animation_frame_; }
    std::uint32_t animation_joints() const {
        return animation_joints_;
    }
    std::uint32_t animation_applications() const {
        return animation_applications_;
    }
    void record_material_animation(
        std::uint32_t resource_id, float frame,
        const dusk::psp::animation::TevRegisterValue* values,
        std::uint32_t count) {
        if (values == nullptr || count > kTevRegisterCapacity) {
            clear_material_animation();
            return;
        }
        material_animation_resource_id_ = resource_id;
        material_animation_frame_ = frame;
        material_register_count_ = count;
        for (std::uint32_t index = 0; index < count; ++index) {
            material_registers_[index] = values[index];
        }
        ++material_animation_applications_;
    }
    void clear_material_animation() {
        material_animation_resource_id_ = 0;
        material_animation_frame_ = 0.0f;
        material_register_count_ = 0;
    }
    std::uint32_t material_animation_resource_id() const {
        return material_animation_resource_id_;
    }
    float material_animation_frame() const {
        return material_animation_frame_;
    }
    std::uint32_t material_register_count() const {
        return material_register_count_;
    }
    const dusk::psp::animation::TevRegisterValue*
    material_registers() const {
        return material_registers_;
    }
    std::uint32_t material_animation_applications() const {
        return material_animation_applications_;
    }

private:
    friend class dusk::psp::model::PspStaticModelRuntime;
    const void* model_bytes_ = nullptr;
    std::uint32_t model_size_ = 0;
    const void* texture_bytes_ = nullptr;
    std::uint32_t texture_size_ = 0;
    std::uint32_t animation_resource_id_ = 0;
    float animation_frame_ = 0.0f;
    std::uint32_t animation_joints_ = 0;
    std::uint32_t animation_applications_ = 0;
    std::uint32_t material_animation_resource_id_ = 0;
    float material_animation_frame_ = 0.0f;
    std::uint32_t material_register_count_ = 0;
    std::uint32_t material_animation_applications_ = 0;
    dusk::psp::animation::TevRegisterValue
        material_registers_[kTevRegisterCapacity] = {};
    J3DJoint joint_stub_;
    J3DMaterialTable material_table_;
    J3DMaterial material_stub_;
};

class J3DAnmTransform {
public:
    bool configure(
        const dusk::psp::playable::PackageView& package,
        std::uint32_t resource_id);
    const dusk::psp::playable::PackageView& package() const;
    std::uint32_t resource_id() const;
    float getFrameMax() const { return frame_max_; }

private:
    dusk::psp::playable::PackageView package_ = {};
    std::uint32_t resource_id_ = 0;
    float frame_max_ = 0.0f;
    bool configured_ = false;
};

class J3DAnmTevRegKey {
public:
    bool configure(
        const dusk::psp::playable::PackageView& package,
        std::uint32_t resource_id);
    const dusk::psp::playable::PackageView& package() const;
    std::uint32_t resource_id() const;
    float getFrameMax() const { return frame_max_; }

private:
    dusk::psp::playable::PackageView package_ = {};
    std::uint32_t resource_id_ = 0;
    float frame_max_ = 0.0f;
    bool configured_ = false;
};

class J3DFrameCtrl {
public:
    enum {
        EMode_NONE = 0,
    };
    bool checkState(int state) const {
        return state == 1 && completed_;
    }
    void setCompleted(bool completed) { completed_ = completed; }

private:
    bool completed_ = false;
};

class mDoExt_baseAnm {
public:
    int play();
    float getPlaySpeed() const;
    void setPlaySpeed(float speed);
    float getFrame() const;
    float getEndFrame() const;
    float getStartFrame() const;
    void setEndFrame(float frame);
    void setFrame(float frame);
    void setPlayMode(int mode);
    bool isStop() const;
    bool isLoop() const;

protected:
    dusk::psp::animation::PspBckPlayer player_;
};

class mDoExt_bckAnm : public mDoExt_baseAnm {
public:
    int init(
        J3DAnmTransform* bck, int play, int attribute,
        float rate, std::int16_t start_frame,
        std::int16_t end_frame, bool modify);
    void changeBckOnly(J3DAnmTransform* bck);
    void entry(J3DModelData* model_data, float frame);
    void entry(J3DModelData* model_data) {
        entry(model_data, getFrame());
    }
    int play() {
        const int result = mDoExt_baseAnm::play();
        frame_control_.setCompleted(isStop());
        return result;
    }
    J3DAnmTransform* getBckAnm();
    J3DFrameCtrl* getFrameCtrl() { return &frame_control_; }

private:
    J3DAnmTransform* animation_ = nullptr;
    J3DFrameCtrl frame_control_;
};

class mDoExt_brkAnm : public mDoExt_baseAnm {
public:
    bool init(
        J3DModelData* model_data, J3DAnmTevRegKey* brk,
        int play, int attribute, float rate,
        std::int16_t start_frame, std::int16_t end_frame);
    int play();
    float getPlaySpeed() const;
    void setPlaySpeed(float speed);
    float getFrame() const;
    float getEndFrame() const;
    float getStartFrame() const;
    void setFrame(float frame);
    void setPlayMode(int mode);
    bool isStop() const;
    bool isLoop() const;
    void entry(J3DModelData* model_data, float frame);
    void entry(J3DModelData* model_data) {
        entry(model_data, getFrame());
    }

private:
    J3DAnmTevRegKey* animation_ = nullptr;
    dusk::psp::animation::PspBrkPlayer brk_player_;
};

inline void mDoExt_brkAnmRemove(J3DModelData* model_data) {
    if (model_data != nullptr) {
        model_data->clear_material_animation();
    }
}

class J3DModel {
public:
    Mtx mBaseTransformMtx = {};

    void setBaseScale(const cXyz& scale);
    void setBaseTRMtx(MtxP matrix);
    Mtx& getBaseTRMtx();
    const Mtx& getBaseTRMtx() const;
    J3DModelData* getModelData();
    const J3DModelData* getModelData() const;
    const cXyz& getBaseScale() const;
    void* owner() const;
    bool active() const;
    void duskPspConfigureStub(J3DModelData* data, void* owner) {
        data_ = data; owner_ = owner; active_ = true;
        std::memset(mBaseTransformMtx, 0, sizeof(mBaseTransformMtx));
        mBaseTransformMtx[0][0] = 1.0f;
        mBaseTransformMtx[1][1] = 1.0f;
        mBaseTransformMtx[2][2] = 1.0f;
    }

private:
    friend class dusk::psp::model::PspStaticModelRuntime;
    J3DModelData* data_ = nullptr;
    void* owner_ = nullptr;
    cXyz scale_ = {1.0f, 1.0f, 1.0f};
    bool active_ = false;
};

J3DModel* mDoExt_J3DModel__create(
    J3DModelData* data, std::uint32_t flags_a, std::uint32_t flags_b);
void mDoExt_modelUpdateDL(J3DModel* model);

#endif
