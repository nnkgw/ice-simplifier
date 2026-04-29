cmake_minimum_required(VERSION 3.16)
SET(CMAKE_BUILD_TYPE Release)
project(ice-simplifier)

# libigl uses FetchContent_Populate() which is deprecated in CMake 4.x (CMP0169)
cmake_policy(SET CMP0169 OLD)

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# libigl (header-only)
include(libigl)

# src/ files
set(INTRINSIC_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/externals/intrinsic-simplification/src")
include_directories(${INTRINSIC_SRC_DIR})

file(GLOB SRC_FILES *.cpp ${INTRINSIC_SRC_DIR}/*.cpp)
list(REMOVE_ITEM SRC_FILES "${INTRINSIC_SRC_DIR}/bake_texture.cpp")

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC igl::core)
if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC "-fno-omit-frame-pointer")
endif()
