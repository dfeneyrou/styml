# Test executable
# ===============

add_executable(styml_unittest)
target_sources(styml_unittest PRIVATE test_main.cpp test_basic.cpp test_access.cpp test_parsing.cpp)
target_link_libraries(styml_unittest PRIVATE libexternal styml)

# Display some build information
add_custom_command(TARGET styml_unittest POST_BUILD
                   COMMENT "Using ${CMAKE_CXX_COMPILER} [${CMAKE_BUILD_TYPE}]" VERBATIM)
