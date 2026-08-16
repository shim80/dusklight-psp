cmake_minimum_required(VERSION 3.25)

project(dusklight_psp_canonical_host_tests LANGUAGES CXX)

set(DUSKLIGHT_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../dusklight-main")
set(DUSK_PSP_ROOT "${DUSKLIGHT_SOURCE_ROOT}/platforms/psp")

add_executable(parity_trace_host_test
    parity_trace_host_test.cpp
    "${DUSK_PSP_ROOT}/src/parity_trace.cpp"
)
target_include_directories(parity_trace_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(parity_trace_host_test PRIVATE cxx_std_20)
target_compile_options(parity_trace_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(frame_profiler_host_test
    frame_profiler_host_test.cpp
    "${DUSK_PSP_ROOT}/src/frame_profiler.cpp"
)

add_executable(startup_runtime_host_test
    startup_runtime_host_test.cpp
    "${DUSK_PSP_ROOT}/src/startup_package.cpp"
    "${DUSK_PSP_ROOT}/src/startup_runtime.cpp"
)

add_executable(startup_ui_host_test
    startup_ui_host_test.cpp
    "${DUSK_PSP_ROOT}/src/startup_ui_package.cpp"
)
add_executable(startup_title_asset_host_test
    startup_title_asset_host_test.cpp
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
add_executable(startup_camera_parity_host_test
    startup_camera_parity_host_test.cpp
    "${DUSK_PSP_ROOT}/src/startup_camera.cpp"
    "${DUSK_PSP_ROOT}/src/startup_camera_track.cpp"
)
target_include_directories(startup_camera_parity_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(startup_camera_parity_host_test PRIVATE cxx_std_20)
target_compile_options(startup_camera_parity_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
add_executable(startup_first_playable_host_test
    startup_first_playable_host_test.cpp
    "${DUSK_PSP_ROOT}/src/actor_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/real_room_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(startup_first_playable_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(startup_first_playable_host_test PRIVATE cxx_std_20)
target_compile_options(startup_first_playable_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
target_include_directories(startup_title_asset_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(startup_title_asset_host_test PRIVATE cxx_std_20)
target_compile_options(startup_title_asset_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
target_include_directories(startup_ui_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(startup_ui_host_test PRIVATE cxx_std_20)
target_compile_options(startup_ui_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
target_include_directories(startup_runtime_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(startup_runtime_host_test PRIVATE cxx_std_20)
target_compile_options(startup_runtime_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
    -UNDEBUG
)
target_include_directories(frame_profiler_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(frame_profiler_host_test PRIVATE cxx_std_20)
target_compile_options(frame_profiler_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(link_fidelity_host_test
    host_link_fidelity_test.cpp
    "${DUSK_PSP_ROOT}/src/real_room_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(link_fidelity_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(link_fidelity_host_test PRIVATE cxx_std_20)
target_compile_options(link_fidelity_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(link_procedure_checkpoint_host_test
    link_procedure_checkpoint_host_test.cpp
    "${DUSK_PSP_ROOT}/src/real_room_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)

add_executable(link_camera_yaw_checkpoint_host_test
    link_camera_yaw_checkpoint_host_test.cpp
    "${DUSK_PSP_ROOT}/src/real_room_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)

add_executable(link_move_speed_checkpoint_host_test
    link_move_speed_checkpoint_host_test.cpp
    "${DUSK_PSP_ROOT}/src/real_room_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(link_move_speed_checkpoint_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(
    link_move_speed_checkpoint_host_test PRIVATE cxx_std_20
)
target_compile_options(link_move_speed_checkpoint_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
target_include_directories(link_camera_yaw_checkpoint_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(
    link_camera_yaw_checkpoint_host_test PRIVATE cxx_std_20
)
target_compile_definitions(
    link_camera_yaw_checkpoint_host_test PRIVATE
    DUSK_REAL_ROOM_CHECKPOINTS=1
)
target_compile_options(link_camera_yaw_checkpoint_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
target_include_directories(link_procedure_checkpoint_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(
    link_procedure_checkpoint_host_test PRIVATE cxx_std_20
)
target_compile_options(link_procedure_checkpoint_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(room_model_collision_parity_host_test
    room_model_collision_parity_host_test.cpp
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(room_model_collision_parity_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(
    room_model_collision_parity_host_test PRIVATE cxx_std_20
)
target_compile_options(room_model_collision_parity_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(original_scene_exit_host_test
    original_scene_exit_host_test.cpp
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_scene_exit.cpp"
    "${DUSK_PSP_ROOT}/src/process_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/original_scene_exit_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(original_scene_exit_host_test PRIVATE
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSKLIGHT_SOURCE_ROOT}/include"
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(original_scene_exit_host_test PRIVATE cxx_std_20)
target_compile_options(original_scene_exit_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
    -Wno-missing-field-initializers -Wno-missing-braces
)

add_executable(original_rendered_actor_host_test
    original_rendered_actor_host_test.cpp
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_scene_exit.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_obj_lv4HsTarget.cpp"
    "${DUSK_PSP_ROOT}/src/actor_heap_arena.cpp"
    "${DUSK_PSP_ROOT}/src/bck_compat.cpp"
    "${DUSK_PSP_ROOT}/src/bck_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/model_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/movebg_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/process_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/original_rendered_actor_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_scene_exit_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/resource_manager.cpp"
    "${DUSK_PSP_ROOT}/src/render_queue.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)

add_executable(render_state_trace_host_test
    render_state_trace_host_test.cpp
    "${DUSK_PSP_ROOT}/src/render_state_trace.cpp"
)
target_include_directories(render_state_trace_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(render_state_trace_host_test PRIVATE cxx_std_20)
target_compile_options(render_state_trace_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
target_include_directories(original_rendered_actor_host_test PRIVATE
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSKLIGHT_SOURCE_ROOT}/include"
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(original_rendered_actor_host_test PRIVATE cxx_std_20)
target_compile_options(original_rendered_actor_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
    -Wno-missing-field-initializers -Wno-missing-braces
    -Wno-uninitialized
)

add_executable(canonical_core_host_test
    canonical_core_host_test.cpp
    "${DUSK_PSP_ROOT}/src/actor_heap_arena.cpp"
    "${DUSK_PSP_ROOT}/src/bck_compat.cpp"
    "${DUSK_PSP_ROOT}/src/bck_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/model_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/movebg_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/resource_manager.cpp"
    "${DUSK_PSP_ROOT}/src/render_queue.cpp"
    "${DUSK_PSP_ROOT}/src/game_context.cpp"
    "${DUSK_PSP_ROOT}/src/environment_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/event_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/interaction_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/item_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/process_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/shadow_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/switch_runtime.cpp"
)
target_include_directories(canonical_core_host_test PRIVATE
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSKLIGHT_SOURCE_ROOT}/include"
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(canonical_core_host_test PRIVATE cxx_std_20)
target_compile_options(canonical_core_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(original_dynamic_actor_host_test
    original_dynamic_actor_host_test.cpp
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_scene_exit.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_obj_lv4HsTarget.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_obj_lv4gear.cpp"
    "${DUSK_PSP_ROOT}/src/actor_heap_arena.cpp"
    "${DUSK_PSP_ROOT}/src/bck_compat.cpp"
    "${DUSK_PSP_ROOT}/src/bck_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/model_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/movebg_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/process_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/original_dynamic_actor_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_rendered_actor_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_scene_exit_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/resource_manager.cpp"
    "${DUSK_PSP_ROOT}/src/render_queue.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(original_dynamic_actor_host_test PRIVATE
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSKLIGHT_SOURCE_ROOT}/include"
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(original_dynamic_actor_host_test PRIVATE cxx_std_20)
target_compile_options(original_dynamic_actor_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
    -Wno-missing-field-initializers -Wno-missing-braces
    -Wno-unused-const-variable
)

add_executable(original_tier_a_actor_host_test
    original_tier_a_actor_host_test.cpp
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_scene_exit.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_tag_poFire.cpp"
    "${DUSK_PSP_ROOT}/src/actor_heap_arena.cpp"
    "${DUSK_PSP_ROOT}/src/bck_compat.cpp"
    "${DUSK_PSP_ROOT}/src/bck_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/model_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/movebg_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/original_scene_exit_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/process_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/resource_manager.cpp"
    "${DUSK_PSP_ROOT}/src/render_queue.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(original_tier_a_actor_host_test PRIVATE
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSKLIGHT_SOURCE_ROOT}/include"
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(original_tier_a_actor_host_test PRIVATE cxx_std_20)
target_compile_options(original_tier_a_actor_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
    -Wno-missing-field-initializers -Wno-missing-braces
    -Wno-unused-const-variable -Wno-unused-variable
)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(original_tier_a_actor_host_test PRIVATE
        -Wno-unused-private-field
    )
endif()

add_executable(switch_surface_host_test
    switch_surface_host_test.cpp
    "${DUSK_PSP_ROOT}/src/switch_runtime.cpp"
)

add_executable(original_tbox_switch_host_test
    original_tbox_switch_host_test.cpp
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_scene_exit.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_tboxSw.cpp"
    "${DUSK_PSP_ROOT}/src/item_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/original_scene_exit_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/process_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
    "${DUSK_PSP_ROOT}/src/switch_runtime.cpp"
)
target_include_directories(original_tbox_switch_host_test PRIVATE
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSKLIGHT_SOURCE_ROOT}/include"
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(original_tbox_switch_host_test PRIVATE cxx_std_20)
target_compile_options(original_tbox_switch_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
    -Wno-missing-field-initializers -Wno-missing-braces
    -Wno-unused-const-variable -Wno-unused-variable
)

add_executable(bck_surface_host_test
    bck_surface_host_test.cpp
    "${DUSK_PSP_ROOT}/src/bck_compat.cpp"
    "${DUSK_PSP_ROOT}/src/bck_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
)
target_include_directories(bck_surface_host_test PRIVATE
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSKLIGHT_SOURCE_ROOT}/include"
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(bck_surface_host_test PRIVATE cxx_std_20)
target_compile_options(bck_surface_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(movebg_surface_host_test
    movebg_surface_host_test.cpp
    "${DUSK_PSP_ROOT}/src/movebg_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(movebg_surface_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(movebg_surface_host_test PRIVATE cxx_std_20)
target_compile_options(movebg_surface_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(original_door_asset_host_test
    original_door_asset_host_test.cpp
    "${DUSK_PSP_ROOT}/src/movebg_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(original_door_asset_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(original_door_asset_host_test PRIVATE cxx_std_20)
target_compile_options(original_door_asset_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(original_spinner_switch_asset_host_test
    original_spinner_switch_asset_host_test.cpp
    "${DUSK_PSP_ROOT}/src/movebg_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(original_spinner_switch_asset_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(
    original_spinner_switch_asset_host_test PRIVATE cxx_std_20
)
target_compile_options(original_spinner_switch_asset_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(original_tbox_asset_host_test
    original_tbox_asset_host_test.cpp
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(original_tbox_asset_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(original_tbox_asset_host_test PRIVATE cxx_std_20)
target_compile_options(original_tbox_asset_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(original_tbox_resource_host_test
    original_tbox_resource_host_test.cpp
    "${DUSK_PSP_ROOT}/src/actor_heap_arena.cpp"
    "${DUSK_PSP_ROOT}/src/bck_compat.cpp"
    "${DUSK_PSP_ROOT}/src/bck_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/model_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/movebg_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/process_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/render_queue.cpp"
    "${DUSK_PSP_ROOT}/src/resource_manager.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(original_tbox_resource_host_test PRIVATE
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSKLIGHT_SOURCE_ROOT}/include"
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(original_tbox_resource_host_test PRIVATE cxx_std_20)
target_compile_options(original_tbox_resource_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(original_tbox_host_test
    original_tbox_host_test.cpp
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_scene_exit.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_tbox.cpp"
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
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/process_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/render_queue.cpp"
    "${DUSK_PSP_ROOT}/src/resource_manager.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(original_tbox_host_test PRIVATE
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSKLIGHT_SOURCE_ROOT}/include"
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(original_tbox_host_test PRIVATE cxx_std_20)
target_compile_options(original_tbox_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
    -Wno-missing-field-initializers -Wno-missing-braces
    -Wno-uninitialized -Wno-unused-const-variable
    -Wno-unused-variable
)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(original_tbox_host_test PRIVATE
        -Wno-unused-private-field
    )
endif()

add_executable(original_door_host_test
    original_door_host_test.cpp
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_scene_exit.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_obj_lv4PoGate.cpp"
    "${DUSK_PSP_ROOT}/src/actor_heap_arena.cpp"
    "${DUSK_PSP_ROOT}/src/bck_compat.cpp"
    "${DUSK_PSP_ROOT}/src/bck_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/model_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/movebg_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/original_door_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_scene_exit_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/process_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/render_queue.cpp"
    "${DUSK_PSP_ROOT}/src/resource_manager.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
    "${DUSK_PSP_ROOT}/src/switch_runtime.cpp"
)

add_executable(original_spinner_switch_host_test
    original_spinner_switch_host_test.cpp
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_scene_exit.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_obj_lv4gear.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_obj_swspinner.cpp"
    "${DUSK_PSP_ROOT}/src/actor_heap_arena.cpp"
    "${DUSK_PSP_ROOT}/src/bck_compat.cpp"
    "${DUSK_PSP_ROOT}/src/bck_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/interaction_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/model_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/movebg_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/original_scene_exit_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_spinner_switch_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/process_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/render_queue.cpp"
    "${DUSK_PSP_ROOT}/src/resource_manager.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
    "${DUSK_PSP_ROOT}/src/switch_runtime.cpp"
)
target_include_directories(original_spinner_switch_host_test PRIVATE
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSKLIGHT_SOURCE_ROOT}/include"
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(
    original_spinner_switch_host_test PRIVATE cxx_std_20
)
target_compile_options(original_spinner_switch_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
    -Wno-missing-field-initializers -Wno-missing-braces
    -Wno-unused-const-variable -Wno-unused-variable
)
target_include_directories(original_door_host_test PRIVATE
    "${DUSK_PSP_ROOT}/compat/include"
    "${DUSKLIGHT_SOURCE_ROOT}/include"
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(original_door_host_test PRIVATE cxx_std_20)
target_compile_options(original_door_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
    -Wno-missing-field-initializers -Wno-missing-braces
    -Wno-unused-const-variable -Wno-unused-variable
)
target_include_directories(switch_surface_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(switch_surface_host_test PRIVATE cxx_std_20)
target_compile_options(switch_surface_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(interaction_surface_host_test
    interaction_surface_host_test.cpp
    "${DUSK_PSP_ROOT}/src/interaction_runtime.cpp"
)

add_executable(event_surface_host_test
    event_surface_host_test.cpp
    "${DUSK_PSP_ROOT}/src/event_runtime.cpp"
)
target_include_directories(event_surface_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(event_surface_host_test PRIVATE cxx_std_20)
target_compile_options(event_surface_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
target_include_directories(interaction_surface_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(
    interaction_surface_host_test PRIVATE cxx_std_20
)
target_compile_options(interaction_surface_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(presentation_profile_host_test
    presentation_profile_host_test.cpp
)
target_include_directories(presentation_profile_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(presentation_profile_host_test PRIVATE cxx_std_20)
target_compile_options(presentation_profile_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(render_profile_host_test
    render_profile_host_test.cpp
)
target_include_directories(render_profile_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(render_profile_host_test PRIVATE cxx_std_20)
target_compile_options(render_profile_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(safe_link_lighting_host_test
    safe_link_lighting_host_test.cpp
)
target_include_directories(safe_link_lighting_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(safe_link_lighting_host_test PRIVATE cxx_std_20)
target_compile_options(safe_link_lighting_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(depth_behavior_contract_host_test
    depth_behavior_contract_host_test.cpp
)
target_include_directories(depth_behavior_contract_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(depth_behavior_contract_host_test PRIVATE cxx_std_20)
target_compile_options(depth_behavior_contract_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(opaque_order_host_test
    opaque_order_host_test.cpp
    "${DUSK_PSP_ROOT}/src/opaque_order.cpp"
)
target_include_directories(opaque_order_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(opaque_order_host_test PRIVATE cxx_std_20)
target_compile_options(opaque_order_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(source_event_script_host_test
    source_event_script_host_test.cpp
    "${DUSK_PSP_ROOT}/src/source_event_script.cpp"
)
target_include_directories(source_event_script_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(source_event_script_host_test PRIVATE cxx_std_20)
target_compile_options(source_event_script_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(source_getitem_camera_host_test
    source_getitem_camera_host_test.cpp
    "${DUSK_PSP_ROOT}/src/source_getitem_camera.cpp"
)
target_include_directories(source_getitem_camera_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(source_getitem_camera_host_test PRIVATE cxx_std_20)
target_compile_options(source_getitem_camera_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(source_message_bmg_host_test
    source_message_bmg_host_test.cpp
    "${DUSK_PSP_ROOT}/src/source_message_bmg.cpp"
)
target_include_directories(source_message_bmg_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(source_message_bmg_host_test PRIVATE cxx_std_20)
target_compile_options(source_message_bmg_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(full_ascii_dpui_host_test
    full_ascii_dpui_host_test.cpp
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
)
target_include_directories(full_ascii_dpui_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(full_ascii_dpui_host_test PRIVATE cxx_std_20)
target_compile_options(full_ascii_dpui_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)

add_executable(alpha_material_state_host_test
    alpha_material_state_host_test.cpp
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
)
target_include_directories(alpha_material_state_host_test PRIVATE
    "${DUSK_PSP_ROOT}/include"
)
target_compile_features(alpha_material_state_host_test PRIVATE cxx_std_20)
target_compile_options(alpha_material_state_host_test PRIVATE
    -Wall -Wextra -Werror -fsigned-char -fno-exceptions -fno-rtti
)
