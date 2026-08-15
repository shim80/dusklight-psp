cmake_minimum_required(VERSION 3.25)

project(dusklight_psp_material_pass_plan_host LANGUAGES CXX)

set(DUSK_PSP_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../dusklight-main/platforms/psp")

add_executable(material_pass_plan_host_test
    ../canonical-runtime/material_pass_plan_host_test.cpp
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(material_pass_plan_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(material_pass_plan_host_test PRIVATE cxx_std_20)
target_compile_options(material_pass_plan_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(startup_title_asset_host_test
    ../canonical-runtime/startup_title_asset_host_test.cpp
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(startup_title_asset_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(startup_title_asset_host_test PRIVATE cxx_std_20)
target_compile_options(startup_title_asset_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
