cmake_minimum_required(VERSION 3.25)

project(dusklight_psp_demo_item_host_regression LANGUAGES CXX)

set(DUSKLIGHT_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../dusklight-main")
set(DUSK_PSP_ROOT "${DUSKLIGHT_SOURCE_ROOT}/platforms/psp")

add_executable(demo_item_commit_host_test
    demo_item_commit_host_test.cpp
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_scene_exit.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_tbox.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_demo_item.cpp"
    "${DUSK_PSP_ROOT}/src/actor_heap_arena.cpp"
    "${DUSK_PSP_ROOT}/src/bck_compat.cpp"
    "${DUSK_PSP_ROOT}/src/bck_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/event_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/source_event_script.cpp"
    "${DUSK_PSP_ROOT}/src/interaction_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/item_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/model_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/movebg_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/original_scene_exit_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_tbox_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_demo_item_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/process_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/render_queue.cpp"
    "${DUSK_PSP_ROOT}/src/resource_manager.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)

target_include_directories(demo_item_commit_host_test PRIVATE
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSKLIGHT_SOURCE_ROOT}/include"
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(demo_item_commit_host_test PRIVATE cxx_std_20)
target_compile_options(demo_item_commit_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
    -Wno-missing-field-initializers -Wno-missing-braces
    -Wno-uninitialized -Wno-maybe-uninitialized
    -Wno-unused-const-variable -Wno-unused-variable
)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(demo_item_commit_host_test PRIVATE
        -Wno-unused-private-field
    )
endif()
