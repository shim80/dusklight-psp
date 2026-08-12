cmake_minimum_required(VERSION 3.25)

set(DUSK_PSP_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(DUSK_PSP_INCLUDE "${DUSK_PSP_ROOT}/include")

function(dusk_psp_library target)
    add_library(${target} STATIC ${ARGN})
    target_include_directories(${target} PUBLIC
        "${DUSKLIGHT_SOURCE_ROOT}/include"
        "${DUSKLIGHT_SOURCE_ROOT}/libs/dolphin/include"
        "${DUSK_PSP_INCLUDE}"
    )
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_options(${target} PRIVATE
        -O2 -G0 -Wall -Wextra -Werror -fsigned-char
        -fno-exceptions -fno-rtti
    )
endfunction()

dusk_psp_library(dusk_psp_platform
    "${DUSK_PSP_ROOT}/src/platform.cpp"
    "${DUSK_PSP_ROOT}/src/frame_profiler.cpp"
    "${DUSK_PSP_ROOT}/src/parity_trace.cpp"
)

dusk_psp_library(dusk_psp_resources
    "${DUSK_PSP_ROOT}/src/playable_package.cpp"
    "${DUSK_PSP_ROOT}/src/room_package.cpp"
    "${DUSK_PSP_ROOT}/src/resource_manager.cpp"
    "${DUSK_PSP_ROOT}/src/startup_package.cpp"
    "${DUSK_PSP_ROOT}/src/startup_ui_package.cpp"
)

dusk_psp_library(dusk_psp_compat
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_scene_exit.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_obj_lv4HsTarget.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_obj_lv4gear.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_obj_lv4PoGate.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_obj_swspinner.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_tag_poFire.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_tbox.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_demo_item.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_tboxSw.cpp"
    "${DUSK_PSP_ROOT}/src/actor_heap_arena.cpp"
    "${DUSK_PSP_ROOT}/src/actor_profile_registry.cpp"
    "${DUSK_PSP_ROOT}/src/model_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/process_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/original_rendered_actor_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_dynamic_actor_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_door_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_scene_exit_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_spinner_switch_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_tbox_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_demo_item_bridge.cpp"
)
set_source_files_properties(
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_tbox.cpp"
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_demo_item.cpp"
    "${DUSK_PSP_ROOT}/src/original_tbox_bridge.cpp"
    "${DUSK_PSP_ROOT}/src/original_demo_item_bridge.cpp"
    PROPERTIES COMPILE_OPTIONS "-g0"
)
target_include_directories(dusk_psp_compat BEFORE PUBLIC
    "${DUSK_PSP_ROOT}/compat/include"
)
target_compile_options(dusk_psp_compat PRIVATE
    -Wno-missing-field-initializers -Wno-missing-braces
    -Wno-uninitialized -Wno-maybe-uninitialized
    -Wno-unused-const-variable -Wno-unused-variable
)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(dusk_psp_compat PRIVATE
        -Wno-unused-private-field
    )
endif()
target_link_libraries(dusk_psp_compat PUBLIC dusk_psp_resources)

dusk_psp_library(dusk_psp_animation
    "${DUSK_PSP_ROOT}/src/bck_compat.cpp"
    "${DUSK_PSP_ROOT}/src/bck_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/playable_runtime.cpp"
)
target_include_directories(dusk_psp_animation BEFORE PUBLIC
    "${DUSK_PSP_ROOT}/compat/include"
)
target_link_libraries(dusk_psp_animation PUBLIC dusk_psp_resources)

dusk_psp_library(dusk_psp_collision
    "${DUSK_PSP_ROOT}/src/movebg_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/room_collision.cpp"
)
target_link_libraries(dusk_psp_collision PUBLIC dusk_psp_resources)
target_link_libraries(dusk_psp_compat PUBLIC dusk_psp_collision)

dusk_psp_library(dusk_psp_stage
    "${DUSK_PSP_ROOT}/src/environment_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/event_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/source_event_script.cpp"
    "${DUSK_PSP_ROOT}/src/source_getitem_camera.cpp"
    "${DUSK_PSP_ROOT}/src/source_message_bmg.cpp"
    "${DUSK_PSP_ROOT}/src/interaction_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/item_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/save_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/save_storage.cpp"
    "${DUSK_PSP_ROOT}/src/stage_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/startup_runtime.cpp"
    "${DUSK_PSP_ROOT}/src/switch_runtime.cpp"
)
target_link_libraries(dusk_psp_stage PUBLIC dusk_psp_resources)

dusk_psp_library(dusk_psp_shadow
    "${DUSK_PSP_ROOT}/src/shadow_runtime.cpp"
)
target_link_libraries(dusk_psp_shadow PUBLIC
    dusk_psp_collision dusk_psp_stage
)

dusk_psp_library(dusk_psp_actor
    "${DUSK_PSP_ROOT}/src/actor_runtime.cpp"
)
target_link_libraries(dusk_psp_actor PUBLIC dusk_psp_resources)

dusk_psp_library(dusk_psp_game
    "${DUSK_PSP_ROOT}/src/real_room_runtime.cpp"
)
target_link_libraries(dusk_psp_game PUBLIC
    dusk_psp_animation dusk_psp_collision dusk_psp_resources
)

dusk_psp_library(dusk_psp_renderer
    "${DUSK_PSP_ROOT}/src/depth_behavior.cpp"
    "${DUSK_PSP_ROOT}/src/opaque_order.cpp"
    "${DUSK_PSP_ROOT}/src/playable_render.cpp"
    "${DUSK_PSP_ROOT}/src/render_state_trace.cpp"
    "${DUSK_PSP_ROOT}/src/startup_camera.cpp"
    "${DUSK_PSP_ROOT}/src/actor_render.cpp"
    "${DUSK_PSP_ROOT}/src/render_queue.cpp"
)
target_include_directories(dusk_psp_renderer BEFORE PUBLIC
    "${DUSK_PSP_ROOT}/compat/include"
)
target_link_libraries(dusk_psp_renderer PUBLIC
    dusk_psp_animation dusk_psp_actor dusk_psp_platform dusk_psp_shadow
)

target_sources(dusk_psp_game PRIVATE
    "${DUSK_PSP_ROOT}/src/game_context.cpp"
)
target_include_directories(dusk_psp_game BEFORE PUBLIC
    "${DUSK_PSP_ROOT}/compat/include"
)
target_link_libraries(dusk_psp_game PUBLIC
    dusk_psp_actor dusk_psp_stage dusk_psp_compat dusk_psp_renderer
)

add_library(dusk_psp_runtime INTERFACE)
target_link_libraries(dusk_psp_runtime INTERFACE
    dusk_psp_platform
    dusk_psp_resources
    dusk_psp_animation
    dusk_psp_collision
    dusk_psp_shadow
    dusk_psp_stage
    dusk_psp_actor
    dusk_psp_game
    dusk_psp_renderer
    dusk_psp_compat
)
