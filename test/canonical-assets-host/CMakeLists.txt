cmake_minimum_required(VERSION 3.25)
project(canonical_assets_host_test LANGUAGES CXX)

set(DUSK_PSP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../dusklight-main/platforms/psp")

add_executable(canonical_assets_host_test
    main.cpp
    "${DUSK_PSP_ROOT}/src/canonical_assets.cpp"
    "${DUSK_PSP_ROOT}/src/save_runtime.cpp"
)
target_include_directories(canonical_assets_host_test PRIVATE "${DUSK_PSP_ROOT}/include")
target_compile_features(canonical_assets_host_test PRIVATE cxx_std_20)
target_compile_options(canonical_assets_host_test PRIVATE -Wall -Wextra -Werror)
