cmake_minimum_required(VERSION 3.25)
project(dusklight_psp_startup_camera_host LANGUAGES CXX)

set(DUSK_PSP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../dusklight-main/platforms/psp")

add_executable(startup_camera_host_test
    main.cpp
    "${DUSK_PSP_ROOT}/src/startup_camera_track.cpp"
)
target_include_directories(startup_camera_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(startup_camera_host_test PRIVATE cxx_std_20)
target_compile_options(startup_camera_host_test PRIVATE
    -Wall -Wextra -Werror -fno-exceptions -fno-rtti
)
