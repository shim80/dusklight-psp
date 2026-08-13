cmake_minimum_required(VERSION 3.25)
project(dusklight_psp_startup_save_integration_host LANGUAGES CXX)

set(DUSKLIGHT_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../dusklight-main")
set(DUSK_PSP_ROOT "${DUSKLIGHT_SOURCE_ROOT}/platforms/psp")

add_executable(startup_save_integration_host_test
    main.cpp
    "${DUSK_PSP_ROOT}/src/save_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/save_storage.cpp"
    "${DUSK_PSP_ROOT}/src/startup_package.cpp"
    "${DUSK_PSP_ROOT}/src/startup_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/startup_save_flow.cpp"
)
target_include_directories(startup_save_integration_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(startup_save_integration_host_test PRIVATE cxx_std_20)
target_compile_options(startup_save_integration_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti -UNDEBUG
)
