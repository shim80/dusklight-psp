cmake_minimum_required(VERSION 3.25)
project(dusklight_psp_reconstructed_canonical LANGUAGES C CXX)

set(DUSKLIGHT_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../dusklight-main")
set(DUSK_PSP_ROOT "${DUSKLIGHT_SOURCE_ROOT}/platforms/psp")

# Reconstructed canonical entry target. The historical test/dusklight-psp
# launcher was lost from the reconstructed repository, while packaging and
# validation scripts still depend on this target/path. Until the asset-backed
# canonical game driver is recovered/rebuilt, reuse the already validated
# startup/save/control PSP entry source so there is one executable boundary
# and no divergent input/save implementation.
add_executable(dusklight_psp
    ../startup-save-psp/main.cpp
    "${DUSK_PSP_ROOT}/src/save_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/save_storage.cpp"
    "${DUSK_PSP_ROOT}/src/startup_package.cpp"
    "${DUSK_PSP_ROOT}/src/startup_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/startup_save_flow.cpp"
    "${DUSK_PSP_ROOT}/src/psp_controls.cpp"
)

target_include_directories(dusklight_psp PRIVATE "${DUSK_PSP_ROOT}/include")
target_compile_features(dusklight_psp PRIVATE cxx_std_20)
target_compile_options(dusklight_psp PRIVATE
    -O2 -G0 -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
target_link_libraries(dusklight_psp PRIVATE pspdebug pspctrl pspdisplay pspge)
set_target_properties(dusklight_psp PROPERTIES OUTPUT_NAME "dusklight_psp.elf")

create_pbp_file(
    TARGET dusklight_psp
    TITLE "Dusklight PSP"
    OUTPUT_DIR "${CMAKE_BINARY_DIR}"
)
