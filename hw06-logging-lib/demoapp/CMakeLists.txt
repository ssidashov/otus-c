cmake_minimum_required(VERSION 3.10)

project(demoapp VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG_OUTPUT")

add_executable(${PROJECT_NAME} demoapp.c)
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_BINARY_DIR}")
configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_BINARY_DIR}/version.h")

target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -Wpedantic)

add_subdirectory("${CMAKE_SOURCE_DIR}/../log4c/" "lib/log4c")
#target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/../log4c/include")

target_link_libraries(${PROJECT_NAME} log4c)
