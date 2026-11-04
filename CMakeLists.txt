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
