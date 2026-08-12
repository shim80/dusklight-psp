#include "d/d_bg_w.h"
#include "d/d_com_inf_game.h"
#include "dusk/psp/model_runtime.hpp"
#include "dusk/psp/movebg_runtime.hpp"
#include "dusk/psp/render_queue.hpp"
#include "dusk/psp/resource_manager.hpp"
#include "m_Do/m_Do_ext.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

bool read_resource(
    void* user, const char* path, void* output,
    std::uint32_t capacity, std::uint32_t* size) {
    const auto* root = static_cast<const std::string*>(user);
    const char* relative =
        std::strncmp(path, "./data/", 7) == 0 ? path + 7 : path;
    const std::vector<std::uint8_t> bytes =
        read_file(*root + "/" + relative);
    if (bytes.empty() || bytes.size() > capacity) {
        return false;
    }
    std::memcpy(output, bytes.data(), bytes.size());
    *size = static_cast<std::uint32_t>(bytes.size());
    return true;
}

bool run_test(const std::string& root) {
    const auto manifest = read_file(root + "/RESOURCE.MANIFEST");
    if (manifest.empty()) {
        return false;
    }
    dusk::psp::resources::PspResourceManager resources;
    dusk::psp::render::PspRenderQueue queue;
    dusk::psp::movebg::PspMoveBgWorld world;
    dusk::psp::model::PspStaticModelRuntime models;
    queue.initialize();
    world.initialize();
    if (!resources.initialize(
            ".", manifest.data(),
            static_cast<std::uint32_t>(manifest.size()),
            read_resource, const_cast<std::string*>(&root)) ||
        !models.initialize(&resources, &queue, &world)) {
        return false;
    }
    dusk::psp::model::bind_model_runtime(&models);

    request_of_phase_process_class phase = {};
    bool valid =
        dComIfG_resLoad(&phase, "Dalways") == cPhs_LOADING_e &&
        dComIfG_resLoad(&phase, "Dalways") == cPhs_COMPLEATE_e;
    auto* model = static_cast<J3DModelData*>(
        dComIfG_getObjectRes("Dalways", 13));
    auto* animation = static_cast<J3DAnmTransform*>(
        dComIfG_getObjectRes("Dalways", 8));
    auto* closed = static_cast<cBgD_t*>(
        dComIfG_getObjectRes("Dalways", 27));
    auto* open = static_cast<cBgD_t*>(
        dComIfG_getObjectRes("Dalways", 28));
    valid = valid && model != nullptr && animation != nullptr &&
            animation->resource_id() == 8 &&
            closed != nullptr && open != nullptr && closed != open;

    Mtx matrix = {};
    matrix[0][0] = 1.0f;
    matrix[1][1] = 1.0f;
    matrix[2][2] = 1.0f;
    dBgW closed_world;
    dBgW open_world;
    valid = valid &&
        closed_world.Set(closed, 1, &matrix) == 0 &&
        dComIfG_Bgsp().Regist(&closed_world, nullptr) == 0;
    closed_world.Move();
    valid = valid &&
        dComIfG_Bgsp().Release(&closed_world) == 0 &&
        open_world.Set(open, 1, &matrix) == 0 &&
        dComIfG_Bgsp().Regist(&open_world, nullptr) == 0;
    open_world.Move();
    valid = valid &&
        dComIfG_Bgsp().Release(&open_world) == 0 &&
        dComIfG_resDelete(&phase, "Dalways") == 1 &&
        models.reference_count() == 0 &&
        models.metrics.errors == 0 &&
        resources.metrics.errors == 0 &&
        world.metrics.creates == 2 &&
        world.metrics.updates == 2 &&
        world.metrics.deletes == 2 &&
        world.metrics.dynamic_collision_frame_lag == 0;

    if (!valid) {
        std::fprintf(
            stderr,
            "phase=%u model=%p animation=%p animation_id=%u "
            "closed=%p open=%p model_errors=%u resource_errors=%u "
            "resource_loads=%u refs=%u world=%u/%u/%u\n",
            phase.state, static_cast<void*>(model),
            static_cast<void*>(animation),
            animation != nullptr ? animation->resource_id() : 0,
            static_cast<void*>(closed), static_cast<void*>(open),
            models.metrics.errors, resources.metrics.errors,
            resources.metrics.load_calls, models.reference_count(),
            world.metrics.creates, world.metrics.updates,
            world.metrics.deletes);
    }
    dusk::psp::model::unbind_model_runtime();
    models.shutdown();
    world.shutdown();
    queue.shutdown();
    resources.shutdown();
    return valid;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 || !run_test(argv[1])) {
        std::fputs("ORIGINAL_TBOX_RESOURCE_HOST_FAILED\n", stderr);
        return 1;
    }
    std::puts(
        "ORIGINAL_TBOX_RESOURCE_HOST_OK archive=Dalways "
        "model=13 animation=8 collisions=27,28 "
        "collision_swaps=2 frame_lag=0");
    return 0;
}
