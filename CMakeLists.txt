cmake_minimum_required(VERSION 3.24)
project(latch VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
    message(FATAL_ERROR "latch requires GCC 16 or newer (found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LATCH_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

function(latch_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(LATCH_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()

add_library(latch_compiler INTERFACE)
target_include_directories(latch_compiler INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(latch_compiler INTERFACE cxx_std_26)

enable_testing()

add_library(latch_version STATIC src/version.cpp)
target_link_libraries(latch_version PUBLIC latch_compiler)
target_compile_definitions(latch_version PUBLIC LATCH_VERSION="${PROJECT_VERSION}")
latch_warnings(latch_version)

find_package(yaml-cpp REQUIRED)
if(TARGET yaml-cpp::yaml-cpp)
    set(LATCH_YAML yaml-cpp::yaml-cpp)
else()
    set(LATCH_YAML yaml-cpp)
endif()

add_library(latch_config STATIC src/config.cpp src/contrast.cpp)
target_link_libraries(latch_config PUBLIC latch_compiler ${LATCH_YAML})
latch_warnings(latch_config)

add_library(latch_state STATIC src/lock_state.cpp src/scale.cpp)
target_link_libraries(latch_state PUBLIC latch_compiler)
latch_warnings(latch_state)

add_executable(lock_state_test tests/lock_state_test.cpp)
target_link_libraries(lock_state_test PRIVATE latch_state)
target_include_directories(lock_state_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
latch_warnings(lock_state_test)
add_test(NAME lock_state COMMAND lock_state_test)

add_executable(scale_test tests/scale_test.cpp)
target_link_libraries(scale_test PRIVATE latch_state)
target_include_directories(scale_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
latch_warnings(scale_test)
add_test(NAME scale COMMAND scale_test)

add_executable(config_test tests/config_test.cpp)
target_link_libraries(config_test PRIVATE latch_config)
target_include_directories(config_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
latch_warnings(config_test)
add_test(NAME config COMMAND config_test)
