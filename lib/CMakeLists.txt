cmake_minimum_required(VERSION 3.15.0)

project(styml)

add_library(styml INTERFACE)
target_include_directories(styml INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# Include the tests
add_subdirectory(ut)
