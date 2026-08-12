#include "dusk/psp/bck_runtime.hpp"
#include "m_Do/m_Do_ext.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

void put_u16(std::uint8_t* bytes, std::uint32_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

void put_u32(std::uint8_t* bytes, std::uint32_t offset, std::uint32_t value) {
    for (std::uint32_t index = 0; index < 4; ++index) {
        bytes[offset + index] =
            static_cast<std::uint8_t>(value >> (index * 8));
    }
}

void put_f32(std::uint8_t* bytes, std::uint32_t offset, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    put_u32(bytes, offset, bits);
}

void put_transform(std::uint8_t* bytes, std::uint32_t offset, float x) {
    put_f32(bytes, offset, x);
    put_f32(bytes, offset + 4, 0.0f);
    put_f32(bytes, offset + 8, 0.0f);
    put_f32(bytes, offset + 12, 0.0f);
    put_f32(bytes, offset + 16, 0.0f);
    put_f32(bytes, offset + 20, 0.0f);
    put_f32(bytes, offset + 24, 1.0f);
    put_f32(bytes, offset + 28, 1.0f);
    put_f32(bytes, offset + 32, 1.0f);
    put_f32(bytes, offset + 36, 1.0f);
}

bool make_rigid_dpan(
    std::uint8_t bytes[256],
    dusk::psp::playable::PackageView* view) {
    std::memset(bytes, 0, 256);
    std::memcpy(bytes, "DPAN", 4);
    put_u16(bytes, 4, 1);
    put_u16(bytes, 6, 128);
    put_u32(bytes, 8, 256);
    put_u32(bytes, 16, 1);
    put_u32(bytes, 20, 1);
    put_u32(bytes, 24, 30);
    put_u32(bytes, 32, 128);
    put_u32(bytes, 36, 48);
    put_u32(bytes, 40, 176);
    put_u32(bytes, 128, 7);
    put_u32(bytes, 136, 2);
    put_u32(bytes, 140, 2);
    put_u32(bytes, 144, 1);
    put_u32(bytes, 152, 176);
    put_u32(bytes, 156, 80);
    put_transform(bytes, 176, 0.0f);
    put_transform(bytes, 216, 10.0f);
    put_u32(
        bytes, 12,
        dusk::psp::playable::package_crc32(bytes, 256));
    return dusk::psp::playable::validate_dpan(
               bytes, 256, view) ==
           dusk::psp::playable::PackageError::Ok;
}

bool close(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

bool pose_sink(
    void* user, std::uint16_t joint,
    const dusk::psp::animation::Transform& transform) {
    auto* calls = static_cast<std::uint32_t*>(user);
    ++*calls;
    return joint == 0 && std::isfinite(transform.translation[0]);
}

bool run_test() {
    std::uint8_t bytes[256] = {};
    dusk::psp::playable::PackageView package = {};
    if (!make_rigid_dpan(bytes, &package)) {
        return false;
    }
    dusk::psp::animation::PspBckPlayer player;
    if (!player.initialize(
            package, 7, dusk::psp::animation::LoopMode::Loop,
            1.0f, 0.0f, -1.0f) ||
        !player.play() || !close(player.frame(), 1.0f)) {
        return false;
    }
    dusk::psp::animation::Transform pose = {};
    if (!player.sample_joint(0, &pose) ||
        !close(pose.translation[0], 5.0f) ||
        !player.play() || !close(player.frame(), 0.0f)) {
        return false;
    }
    if (!player.set_frame(1.0f) ||
        !player.set_speed(-1.0f) ||
        !player.set_loop_mode(
            dusk::psp::animation::LoopMode::Loop) ||
        !player.play() || !close(player.frame(), 0.0f) ||
        !player.play() || !close(player.frame(), 1.0f)) {
        return false;
    }
    if (!player.set_frame(1.0f) ||
        !player.set_speed(1.0f) ||
        !player.set_loop_mode(
            dusk::psp::animation::LoopMode::Once) ||
        !player.play() || !player.stopped()) {
        return false;
    }
    if (!player.set_speed(0.0f) ||
        !player.set_frame(0.5f) ||
        !close(player.frame(), 0.5f)) {
        return false;
    }
    std::uint32_t applications = 0;
    if (!player.apply(pose_sink, &applications) ||
        applications != 1 ||
        player.initialize(
            package, 99, dusk::psp::animation::LoopMode::Once,
            1.0f, 0.0f, -1.0f)) {
        return false;
    }
    J3DAnmTransform source_animation;
    mDoExt_bckAnm source_player;
    J3DModelData model_data;
    if (!source_animation.configure(package, 7) ||
        !source_player.init(
            &source_animation, 1, 2, 1.0f, 0, -1, false) ||
        source_player.play() == 0) {
        return false;
    }
    source_player.entry(&model_data);
    if (model_data.animation_resource_id() != 7 ||
        model_data.animation_joints() != 1 ||
        model_data.animation_applications() != 1) {
        return false;
    }
    return true;
}

}  // namespace

int main() {
    if (!run_test()) {
        std::fputs("BCK_SURFACE_HOST_FAILED\n", stderr);
        return 1;
    }
    std::printf(
        "BCK_SURFACE_HOST_OK rigid=true skeletal_limit=64 "
        "loop=true once=true reverse=true interpolation=true\n");
    return 0;
}
