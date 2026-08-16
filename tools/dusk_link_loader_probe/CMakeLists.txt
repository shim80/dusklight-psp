cmake_minimum_required(VERSION 3.23)
project(dusk_link_loader_probe LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(AURORA_COMMIT "81f12f31d23ec822d8bde2031c91e94c470911eb")
set(NOD_COMMIT "dc18d2ff129f05228b8510ea092d8b24c290a49a")
set(AURORA_ROOT "${PROJECT_ROOT}/.tools/sources/aurora/${AURORA_COMMIT}")
set(NOD_ROOT "${PROJECT_ROOT}/.tools/sources/nod/${NOD_COMMIT}")
set(DUSKLIGHT_ROOT "${PROJECT_ROOT}/dusklight-main")
set(NOD_LIBRARY "${PROJECT_ROOT}/build/host/link-loader/nod/release/libnod.a")

foreach(required_path IN ITEMS
    "${AURORA_ROOT}/lib/dolphin/dvd/dvd.cpp"
    "${NOD_ROOT}/nod-ffi/include/nod.h"
    "${NOD_LIBRARY}")
  if(NOT EXISTS "${required_path}")
    message(FATAL_ERROR "Missing pinned loader input: ${required_path}")
  endif()
endforeach()

find_package(fmt 12 CONFIG REQUIRED)
find_package(Threads REQUIRED)

set(DUSKLIGHT_LOADER_SOURCES
  "${DUSKLIGHT_ROOT}/src/helpers/string.cpp"
  "${DUSKLIGHT_ROOT}/src/helpers/endian.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JMath/JMath.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JMath/JMATrigonometric.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JSupport/JSUList.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JKernel/JKRHeap.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JKernel/JKRExpHeap.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JKernel/JKRDisposer.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JKernel/JKRFileLoader.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JKernel/JKRFileFinder.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JKernel/JKRArchivePub.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JKernel/JKRArchivePri.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JKernel/JKRMemArchive.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JKernel/JKRDecomp.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/JUtility/JUTNameTab.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DGD.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DSys.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DVertex.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DTransform.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DTexture.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DPacket.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DShapeMtx.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DShapeDraw.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DShape.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DMaterial.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DMatBlock.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DTevs.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DDrawBuffer.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphBase/J3DStruct.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphAnimator/J3DShapeTable.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphAnimator/J3DJointTree.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphAnimator/J3DModelData.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphAnimator/J3DMtxBuffer.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphAnimator/J3DModel.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphAnimator/J3DAnimation.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphAnimator/J3DMaterialAnm.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphAnimator/J3DSkinDeform.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphAnimator/J3DCluster.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphAnimator/J3DJoint.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphAnimator/J3DMaterialAttach.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphLoader/J3DMaterialFactory.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphLoader/J3DMaterialFactory_v21.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphLoader/J3DClusterLoader.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphLoader/J3DModelLoader.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphLoader/J3DModelLoaderCalcSize.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphLoader/J3DJointFactory.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphLoader/J3DShapeFactory.cpp"
  "${DUSKLIGHT_ROOT}/libs/JSystem/src/J3DGraphLoader/J3DAnmLoader.cpp"
  "${AURORA_ROOT}/lib/dolphin/mtx/mtx.c"
  "${AURORA_ROOT}/lib/dolphin/mtx/vec.c"
  "${AURORA_ROOT}/lib/dolphin/gd/GDBase.cpp"
  "${AURORA_ROOT}/lib/dolphin/gd/GDGeometry.cpp"
  "${AURORA_ROOT}/lib/dolphin/gd/GDLight.cpp"
)

add_library(dusklight_loader STATIC ${DUSKLIGHT_LOADER_SOURCES})
target_link_libraries(dusklight_loader PRIVATE fmt::fmt)

target_compile_definitions(dusklight_loader PRIVATE
  AURORA=1
  AVOID_UB=1
  DEBUG=0
  DUSK_BUILDING_GAME=1
  MTX_USE_PS=1
  TARGET_PC=1
  VERSION=1
  WIDESCREEN_SUPPORT=1
)

target_include_directories(dusklight_loader PRIVATE
  include
  "${DUSKLIGHT_ROOT}/include"
  "${DUSKLIGHT_ROOT}/libs/JSystem/include"
  "${DUSKLIGHT_ROOT}/sdk/include"
  "${DUSKLIGHT_ROOT}/src"
  "${DUSKLIGHT_ROOT}/assets/GZ2P01"
  "${AURORA_ROOT}/include/dolphin"
  "${AURORA_ROOT}/include"
  "${AURORA_ROOT}/lib"
)

target_compile_options(dusklight_loader PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include global.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include dolphin/os.h>"
  -ffunction-sections
  -fdata-sections
  -fsigned-char
  -Wno-deprecated-declarations
  -Wno-multichar
  -Wno-non-pod-varargs
  -Wno-trigraphs
)

add_executable(dusk_link_loader_probe
  src/main.cpp
  src/playable_export.cpp
  src/host_compat.cpp
  src/graphics_guard.cpp
  "${AURORA_ROOT}/lib/logging.cpp"
  "${AURORA_ROOT}/lib/dolphin/dvd/dvd.cpp"
  "${AURORA_ROOT}/lib/dolphin/dvd/fst.cpp"
)

target_compile_definitions(dusk_link_loader_probe PRIVATE
  AURORA=1
  AVOID_UB=1
  DEBUG=0
  DUSK_BUILDING_GAME=1
  MTX_USE_PS=1
  TARGET_PC=1
  VERSION=1
  WIDESCREEN_SUPPORT=1
)

target_include_directories(dusk_link_loader_probe PRIVATE
  include
  "${DUSKLIGHT_ROOT}/include"
  "${DUSKLIGHT_ROOT}/libs/JSystem/include"
  "${DUSKLIGHT_ROOT}/sdk/include"
  "${DUSKLIGHT_ROOT}/src"
  "${DUSKLIGHT_ROOT}/assets/GZ2P01"
  "${AURORA_ROOT}/include/dolphin"
  "${AURORA_ROOT}/include"
  "${AURORA_ROOT}/lib"
  "${NOD_ROOT}/nod-ffi/include"
)

target_link_libraries(dusk_link_loader_probe PRIVATE
  dusklight_loader
  "${NOD_LIBRARY}"
  fmt::fmt
  Threads::Threads
)

target_compile_options(dusk_link_loader_probe PRIVATE
  -Wall
  -Wextra
  -Wpedantic
  -Werror=return-type
  -fsigned-char
)

target_link_options(dusk_link_loader_probe PRIVATE -Wl,-dead_strip)
