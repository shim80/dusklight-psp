cmake_minimum_required(VERSION 3.25)
project(dusklight_chest_source_full LANGUAGES C CXX)
set(DUSKLIGHT_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../dusklight-main")
add_subdirectory("${DUSKLIGHT_SOURCE_ROOT}/platforms/psp" "${CMAKE_BINARY_DIR}/psp-runtime")
add_executable(chest_source_full main.cpp)
target_compile_features(chest_source_full PRIVATE cxx_std_20)
target_include_directories(chest_source_full BEFORE PRIVATE
    "${DUSKLIGHT_SOURCE_ROOT}/platforms/psp/compat/include")
target_compile_options(chest_source_full PRIVATE
    -O2 -G0 -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti)

# CMake emits an INTERFACE target's transitive archives after the executable's
# direct libraries. Listing the PSP stubs beside dusk_psp_runtime therefore put
# the stubs first, which psp-fixup-imports rejects. Keep all project archives in
# one rescan group and append the PSPSDK libraries only after that group.
target_link_libraries(chest_source_full PRIVATE
    "-Wl,--start-group"
    dusk_psp_game
    dusk_psp_renderer
    dusk_psp_platform
    dusk_psp_animation
    dusk_psp_shadow
    dusk_psp_stage
    dusk_psp_actor
    dusk_psp_compat
    dusk_psp_collision
    dusk_psp_resources
    "-Wl,--end-group"
    pspgum
    pspgu
    pspge
    pspdisplay
    pspdebug
    pspctrl
)
create_pbp_file(
    TARGET chest_source_full
    TITLE "Dusklight Source Treasure"
    OUTPUT_DIR "${CMAKE_BINARY_DIR}")
