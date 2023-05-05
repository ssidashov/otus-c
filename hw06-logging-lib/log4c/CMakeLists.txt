cmake_minimum_required(VERSION 3.10)

project(log4c VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG_OUTPUT")

find_package(PkgConfig)
pkg_search_module(libunwind REQUIRED IMPORTED_TARGET libunwind >= 1.0)

set(LOG4C_LIB log4c)

file(GLOB LOG4C_HEADERS log4c.h)
file(GLOB LOG4C_SOURCES src/*.c)

add_library("${LOG4C_LIB}" STATIC "${LOG4C_HEADERS}" "${LOG4C_SOURCES}")
target_link_libraries("${LOG4C_LIB}" PUBLIC PkgConfig::libunwind)
target_include_directories("${LOG4C_LIB}" PUBLIC include "${PROJECT_BINARY_DIR}" "${Backtrace_INCLUDE_DIRS}")
configure_file("${PROJECT_SOURCE_DIR}/src/version.h.in" "${PROJECT_BINARY_DIR}/version.h")

target_compile_options("${LOG4C_LIB}" PUBLIC -Wall -Wextra -Wpedantic)
