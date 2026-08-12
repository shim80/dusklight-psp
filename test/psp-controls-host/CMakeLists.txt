cmake_minimum_required(VERSION 3.25)
project(dusklight_psp_controls_host LANGUAGES CXX)

set(DUSKLIGHT_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../dusklight-main")
set(DUSK_PSP_ROOT "${DUSKLIGHT_SOURCE_ROOT}/platforms/psp")

add_executable(psp_controls_host_test
    main.cpp
    "${DUSK_PSP_ROOT}/src/psp_controls.cpp"
)
target_include_directories(psp_controls_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(psp_controls_host_test PRIVATE cxx_std_20)
target_compile_options(psp_controls_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti -UNDEBUG
)
