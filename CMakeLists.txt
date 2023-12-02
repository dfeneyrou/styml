cmake_minimum_required(VERSION 3.15.0)

project(STYML VERSION 1.0.0
        DESCRIPTION "An efficient C++ single-header STrictYaML parser and emitter")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use ccache if available
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif(CCACHE_PROGRAM)

# Set canonical output directories
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Type of build" FORCE)
endif()
message("Build type: ${CMAKE_BUILD_TYPE}")

# Options
# =======

if(MSVC)
  add_compile_options(/W4 /permissive-)
  add_compile_options(/wd4996) # Disable the "This function or variable may be unsafe", pushing for
                               # not well supported extensions
else(MSVC)

  add_compile_options(-Wall -Wno-unused-function -Wconversion -Wextra -Wpedantic)

  if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
    message("Address sanitizer is enabled.")
  endif()

  if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
    message("Undefined Behavior sanitizer is enabled.")
  endif()

endif(MSVC)

# System flags
# ============

# Register our package finders
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

# Project
# =======

# External dependencies (interfaces)
add_subdirectory(external)

# Header only library and its tests
add_subdirectory(lib)

# Associated tools
add_subdirectory(apps)

# Custom target to call tests
add_custom_target(test ${CMAKE_BINARY_DIR}/bin/styml_unittest DEPENDS styml_unittest)
