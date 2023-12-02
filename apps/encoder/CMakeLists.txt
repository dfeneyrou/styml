project(styml_encoder)

add_executable(styml_encoder)
target_sources(styml_encoder PRIVATE encoder.cpp)
target_link_libraries(styml_encoder PRIVATE styml)
