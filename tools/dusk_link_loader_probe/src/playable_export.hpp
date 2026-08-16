#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class J3DModelData;
class J3DAnmTransform;

void export_playable_model_and_animations(
    const std::array<J3DModelData*, 4>& models,
    std::vector<std::uint8_t>& animation_archive);

struct HudSourceResource {
    std::string name;
    std::vector<std::uint8_t> bytes;
};

void export_original_hud_package(
    const std::vector<std::uint8_t>& layout,
    const std::vector<HudSourceResource>& resources,
    const std::vector<std::uint8_t>& font);

void export_original_startup_ui_packages(
    const std::vector<HudSourceResource>& logo_resources,
    const std::vector<HudSourceResource>& warning_resources,
    const std::vector<std::uint8_t>& font);
void export_original_file_select_ui_package(
    const std::vector<HudSourceResource>& resources,
    const char* output);

void export_real_room_packages(
    J3DModelData* model,
    const std::uint8_t* collision,
    std::uint32_t collision_size,
    const std::uint8_t* placement,
    std::uint32_t placement_size,
    const std::uint8_t* stage,
    std::uint32_t stage_size);

void export_static_model_packages(
    J3DModelData* model,
    const char* dprm_path,
    const char* dptx_path);

void export_animated_static_model_package(
    J3DModelData* model,
    J3DAnmTransform* animation,
    float frame,
    const char* dprm_path);

void export_static_model_texture_package(
    J3DModelData* model,
    const char* dptx_path);

void export_static_movebg_packages(
    J3DModelData* model,
    const std::uint8_t* collision,
    std::uint32_t collision_size,
    const char* dprm_path,
    const char* dptx_path,
    const char* dpcl_path);

void export_movebg_collision_package(
    const std::uint8_t* collision,
    std::uint32_t collision_size,
    const char* dpcl_path);

void export_static_bck_package(
    std::vector<std::uint8_t>& archive_bytes,
    std::uint16_t resource_id,
    const char* dpan_path);
