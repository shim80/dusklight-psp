cmake_minimum_required(VERSION 3.25)
project(dusklight_psp_reconstructed_canonical LANGUAGES C CXX)

set(DUSKLIGHT_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../dusklight-main")
set(DUSK_PSP_ROOT "${DUSKLIGHT_SOURCE_ROOT}/platforms/psp")

# The reconstructed public tree intentionally does not contain the complete
# pinned upstream Dusklight source tree. The platform-level dusk_psp_runtime
# target also builds compatibility actors from that missing upstream tree, so
# this canonical executable uses the gameplay/renderer subset that is wholly
# owned by the reconstructed PSP overlay. The renderer list mirrors the
# official dusk_psp_renderer target and deliberately excludes the separate
# upstream-dependent static render bridge. Asset-backed builds assembled from
# the pinned sources remain the parity/release authority.
add_executable(dusklight_psp
    main.cpp
    "${DUSK_PSP_ROOT}/src/canonical_game.cpp"
    "${DUSK_PSP_ROOT}/src/platform.cpp"
    "${DUSK_PSP_ROOT}/src/canonical_assets.cpp"
    "${DUSK_PSP_ROOT}/src/canonical_common_loader.cpp"
    "${DUSK_PSP_ROOT}/src/canonical_room_loader.cpp"

    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/playable_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/actor_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/real_room_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
    "${DUSK_PSP_ROOT}/src/environment_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/shadow_runtime.cpp"

    "${DUSK_PSP_ROOT}/src/depth_behavior.cpp"
    "${DUSK_PSP_ROOT}/src/opaque_order.cpp"
    "${DUSK_PSP_ROOT}/src/playable_render.cpp"
    "${DUSK_PSP_ROOT}/src/render_queue.cpp"
    "${DUSK_PSP_ROOT}/src/render_state_trace.cpp"
    "${DUSK_PSP_ROOT}/src/startup_camera.cpp"
    "${DUSK_PSP_ROOT}/src/actor_render.cpp"

    "${DUSK_PSP_ROOT}/src/save_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/save_storage.cpp"
    "${DUSK_PSP_ROOT}/src/startup_package.cpp"
    "${DUSK_PSP_ROOT}/src/startup_ui_package.cpp"
    "${DUSK_PSP_ROOT}/src/startup_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/startup_save_flow.cpp"
    "${DUSK_PSP_ROOT}/src/psp_controls.cpp"
)

target_include_directories(dusklight_psp PRIVATE "${DUSK_PSP_ROOT}/include")
target_compile_features(dusklight_psp PRIVATE cxx_std_20)
target_compile_options(dusklight_psp PRIVATE
    -O2 -G0 -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
target_link_libraries(dusklight_psp PRIVATE
    pspdebug pspctrl pspdisplay pspge pspgu pspgum
)
set_target_properties(dusklight_psp PROPERTIES OUTPUT_NAME "dusklight_psp.elf")

create_pbp_file(
    TARGET dusklight_psp
    TITLE "Dusklight PSP"
    OUTPUT_DIR "${CMAKE_BINARY_DIR}"
)
