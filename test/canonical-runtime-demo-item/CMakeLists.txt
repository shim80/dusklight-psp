cmake_minimum_required(VERSION 3.25)

project(dusklight_psp_demo_item_host_regression LANGUAGES CXX)

# Reuse the canonical host graph, then complete the source list required by the
# stacked Demo_Item lifecycle work. The base branch's original_tbox_host_test
# references Demo_Item symbols but does not compile their source/PSP bridge.
add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../canonical-runtime"
    "${CMAKE_BINARY_DIR}/canonical-runtime"
)

target_sources(original_tbox_host_test PRIVATE
    "${DUSKLIGHT_SOURCE_ROOT}/src/d/actor/d_a_demo_item.cpp"
    "${DUSK_PSP_ROOT}/src/original_demo_item_bridge.cpp"
)

target_compile_options(original_tbox_host_test PRIVATE
    -Wno-maybe-uninitialized
)
