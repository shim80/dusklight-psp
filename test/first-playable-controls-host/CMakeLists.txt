cmake_minimum_required(VERSION 3.25)
project(first_playable_controls_host LANGUAGES CXX)

set(DUSK_PSP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../dusklight-main/platforms/psp")

add_executable(first_playable_controls_host_test
    main.cpp
    "${DUSK_PSP_ROOT}/src/first_playable_controls.cpp"
)

target_include_directories(first_playable_controls_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(first_playable_controls_host_test PRIVATE cxx_std_20)
target_compile_options(first_playable_controls_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
