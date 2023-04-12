cmake_minimum_required(VERSION 3.10)

project(gstreamerwavsrc VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG_OUTPUT")

find_package(PkgConfig)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-base REQUIRED IMPORTED_TARGET gstreamer-base-1.0>=1.4)

add_library(gstwavsrc SHARED gstwavsrc.h gstwavsrc.c)
target_include_directories(gstwavsrc PUBLIC "${PROJECT_BINARY_DIR}")
configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_BINARY_DIR}/version.h")

target_compile_options(gstwavsrc PUBLIC -Wall -Wextra -Wpedantic)

target_link_libraries(gstwavsrc PkgConfig::gstreamer PkgConfig::gstreamer-base)
