cmake_minimum_required(VERSION 3.10)

project(curlweather VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG_OUTPUT")

add_executable(${PROJECT_NAME} wttrrequest.c wttrrequest.h curlweather.c)
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_BINARY_DIR}")
configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_BINARY_DIR}/version.h")

target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -Wpedantic)

find_package(CURL REQUIRED)

set(ENABLE_CJSON_TEST Off CACHE BOOL "")
set(ENABLE_CUSTOM_COMPILER_FLAGS Off CACHE BOOL "")
set(BUILD_SHARED_LIBS Off CACHE BOOL "")
add_subdirectory("3rdparty/cJSON")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/3rdparty/cJSON")

target_link_libraries(curlweather ${CURL_LIBRARIES} cjson)
