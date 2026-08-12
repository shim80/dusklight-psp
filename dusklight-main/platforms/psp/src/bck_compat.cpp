#include "m_Do/m_Do_ext.h"

namespace {

bool valid_mode(int mode) {
    return mode >= 0 && mode <= 4;
}

bool consume_pose(
    void*, std::uint16_t,
    const dusk::psp::animation::Transform&) {
    return true;
}

}  // namespace

bool J3DAnmTransform::configure(
    const dusk::psp::playable::PackageView& package,
    std::uint32_t resource_id) {
    dusk::psp::animation::PspBckPlayer probe;
    if (!probe.initialize(
            package, resource_id,
            dusk::psp::animation::LoopMode::Once,
            0.0f, 0.0f, -1.0f)) {
        return false;
    }
    package_ = package;
    resource_id_ = resource_id;
    frame_max_ = static_cast<float>(probe.clip().frame_max);
    configured_ = true;
    return true;
}

const dusk::psp::playable::PackageView&
J3DAnmTransform::package() const {
    return package_;
}

std::uint32_t J3DAnmTransform::resource_id() const {
    return configured_ ? resource_id_ : 0;
}

int mDoExt_baseAnm::play() {
    return player_.play() ? 1 : 0;
}

float mDoExt_baseAnm::getPlaySpeed() const {
    return player_.speed();
}

void mDoExt_baseAnm::setPlaySpeed(float speed) {
    player_.set_speed(speed);
}

float mDoExt_baseAnm::getFrame() const {
    return player_.frame();
}

float mDoExt_baseAnm::getEndFrame() const {
    return player_.end_frame();
}

float mDoExt_baseAnm::getStartFrame() const {
    return player_.start_frame();
}

void mDoExt_baseAnm::setEndFrame(float frame) {
    const float current = player_.frame();
    const float speed = player_.speed();
    const auto mode = player_.loop_mode();
    const auto clip = player_.clip();
    if (clip.valid && player_.initialize(
            clip.package, clip.resource_id, mode, speed,
            player_.start_frame(), frame)) {
        player_.set_frame(current < frame ? current :
                          player_.start_frame());
    }
}

void mDoExt_baseAnm::setFrame(float frame) {
    player_.set_frame(frame);
}

void mDoExt_baseAnm::setPlayMode(int mode) {
    if (valid_mode(mode)) {
        player_.set_loop_mode(
            static_cast<dusk::psp::animation::LoopMode>(mode));
    }
}

bool mDoExt_baseAnm::isStop() const {
    return player_.stopped();
}

bool mDoExt_baseAnm::isLoop() const {
    return player_.loop_mode() ==
               dusk::psp::animation::LoopMode::Loop ||
           player_.loop_mode() ==
               dusk::psp::animation::LoopMode::LoopReverse;
}

int mDoExt_bckAnm::init(
    J3DAnmTransform* bck, int play, int attribute,
    float rate, std::int16_t start_frame,
    std::int16_t end_frame, bool) {
    if (bck == nullptr || !valid_mode(attribute) ||
        !player_.initialize(
            bck->package(), bck->resource_id(),
            static_cast<dusk::psp::animation::LoopMode>(attribute),
            play != 0 ? rate : 0.0f,
            static_cast<float>(start_frame),
            static_cast<float>(end_frame))) {
        return 0;
    }
    animation_ = bck;
    return 1;
}

void mDoExt_bckAnm::changeBckOnly(J3DAnmTransform* bck) {
    if (bck != nullptr &&
        player_.change_clip(bck->package(), bck->resource_id())) {
        animation_ = bck;
    }
}

void mDoExt_bckAnm::entry(
    J3DModelData* model_data, float frame) {
    if (model_data == nullptr || animation_ == nullptr ||
        !player_.set_frame(frame) ||
        !player_.apply(consume_pose, nullptr)) {
        return;
    }
    model_data->record_animation(
        animation_->resource_id(), frame,
        player_.clip().joints);
}

J3DAnmTransform* mDoExt_bckAnm::getBckAnm() {
    return animation_;
}

bool dusk::psp::animation::source_compatibility_surface_valid() {
    return sizeof(J3DAnmTransform) != 0 &&
           sizeof(mDoExt_bckAnm) != 0;
}
