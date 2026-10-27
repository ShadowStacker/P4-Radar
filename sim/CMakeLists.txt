# Desktop simulator for the Plane Radar P4 UI.
#
#   cmake -S sim -B build-sim && cmake --build build-sim -j
#   ./build-sim/plane_radar_sim sim_out
#
# LVGL and cJSON are fetched from GitHub unless LVGL_DIR / CJSON_DIR point at
# local checkouts.
cmake_minimum_required(VERSION 3.16)
project(plane_radar_sim C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(LVGL_DIR "" CACHE PATH "Local LVGL 9.x checkout (optional)")
set(CJSON_DIR "" CACHE PATH "Local cJSON checkout (optional)")
set(LVGL_TAG "v9.4.0" CACHE STRING "LVGL tag to fetch")

include(FetchContent)
if(NOT LVGL_DIR)
  FetchContent_Declare(lvgl GIT_REPOSITORY https://github.com/lvgl/lvgl.git GIT_TAG ${LVGL_TAG} GIT_SHALLOW TRUE)
  FetchContent_Populate(lvgl)
  set(LVGL_DIR ${lvgl_SOURCE_DIR})
endif()
if(NOT CJSON_DIR)
  FetchContent_Declare(cjson GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git GIT_TAG v1.7.18 GIT_SHALLOW TRUE)
  FetchContent_Populate(cjson)
  set(CJSON_DIR ${cjson_SOURCE_DIR})
endif()

find_package(ZLIB REQUIRED)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

file(GLOB_RECURSE LVGL_SOURCES ${LVGL_DIR}/src/*.c)
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC ${LVGL_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)

add_library(cjson STATIC ${CJSON_DIR}/cJSON.c)
target_include_directories(cjson PUBLIC ${CJSON_DIR})

file(GLOB APP_SOURCES
  ${ROOT}/main/core/*.cpp
  ${ROOT}/main/app/*.cpp
  ${ROOT}/main/ui/*.cpp
  ${ROOT}/main/data/*.cpp)

add_executable(plane_radar_sim ${APP_SOURCES} sim_main.cpp platform_sim.cpp fake_traffic.cpp)
target_include_directories(plane_radar_sim PRIVATE ${ROOT}/main ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(plane_radar_sim PRIVATE lvgl cjson ZLIB::ZLIB pthread m)
target_compile_options(plane_radar_sim PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)

# Host unit tests for the pure core logic.
file(GLOB CORE_SOURCES ${ROOT}/main/core/*.cpp)
add_executable(core_tests ${ROOT}/tests/core_tests.cpp ${CORE_SOURCES})
target_include_directories(core_tests PRIVATE ${ROOT}/main)
target_link_libraries(core_tests PRIVATE cjson ZLIB::ZLIB m pthread)
target_compile_options(core_tests PRIVATE -Wall -Wextra)
