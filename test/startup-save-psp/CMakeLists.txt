cmake_minimum_required(VERSION 3.25)
project(dusklight_psp_startup_save LANGUAGES C CXX)

set(DUSKLIGHT_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../dusklight-main")
set(DUSK_PSP_ROOT "${DUSKLIGHT_SOURCE_ROOT}/platforms/psp")

add_executable(startup_save_psp
    main.cpp
    "${DUSK_PSP_ROOT}/src/save_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/save_storage.cpp"
    "${DUSK_PSP_ROOT}/src/startup_package.cpp"
    "${DUSK_PSP_ROOT}/src/startup_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/startup_save_flow.cpp"
)
target_include_directories(startup_save_psp PRIVATE "${DUSK_PSP_ROOT}/include")
target_compile_features(startup_save_psp PRIVATE cxx_std_20)
target_compile_options(startup_save_psp PRIVATE
    -O2 -G0 -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
target_link_libraries(startup_save_psp PRIVATE pspdebug pspctrl pspdisplay pspge)
create_pbp_file(
    TARGET startup_save_psp
    TITLE "Dusklight PSP - Quest Log"
    OUTPUT_DIR "${CMAKE_BINARY_DIR}"
)
