cmake_minimum_required(VERSION 3.25)
project(dusklight_psp_reconstructed_canonical LANGUAGES C CXX)

set(DUSKLIGHT_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../dusklight-main")
set(DUSK_PSP_ROOT "${DUSKLIGHT_SOURCE_ROOT}/platforms/psp")

# Reuse the project's canonical PSP library graph so the reconstructed EBOOT
# compiles against the same gameplay/animation/renderer dependencies as the
# rest of the port instead of maintaining a divergent hand-written source
# list here.
add_subdirectory(
    "${DUSK_PSP_ROOT}"
    "${CMAKE_BINARY_DIR}/dusklight-psp-runtime"
)

add_executable(dusklight_psp
    main.cpp
    "${DUSK_PSP_ROOT}/src/canonical_game.cpp"
    "${DUSK_PSP_ROOT}/src/canonical_assets.cpp"
    "${DUSK_PSP_ROOT}/src/canonical_common_loader.cpp"
    "${DUSK_PSP_ROOT}/src/canonical_room_loader.cpp"
    "${DUSK_PSP_ROOT}/src/psp_controls.cpp"
)

target_include_directories(dusklight_psp PRIVATE "${DUSK_PSP_ROOT}/include")
target_compile_features(dusklight_psp PRIVATE cxx_std_20)
target_compile_options(dusklight_psp PRIVATE
    -O2 -G0 -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
target_link_libraries(dusklight_psp PRIVATE
    dusk_psp_runtime
    pspdebug pspctrl pspdisplay pspge pspgu pspgum
)
set_target_properties(dusklight_psp PROPERTIES OUTPUT_NAME "dusklight_psp.elf")

create_pbp_file(
    TARGET dusklight_psp
    TITLE "Dusklight PSP"
    OUTPUT_DIR "${CMAKE_BINARY_DIR}"
)
