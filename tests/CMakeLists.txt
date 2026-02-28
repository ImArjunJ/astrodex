add_executable(data_aggregation_tests
    data/test_nasa_client.cpp
    data/test_oec_parser.cpp
    data/test_gaia_client.cpp
    data/test_cds_client.cpp
    data/test_coordinate_matcher.cpp
    data/test_data_fusion.cpp
    data/test_inference_integration.cpp
)

target_link_libraries(data_aggregation_tests
    PRIVATE
        astrocore_lib
        nlohmann_json::nlohmann_json
        Catch2::Catch2WithMain
)

target_include_directories(data_aggregation_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

include(CTest)
include(Catch)
catch_discover_tests(data_aggregation_tests)

# Render parameter mapping tests
add_executable(render_tests
    render/test_planet_params.cpp
)

target_link_libraries(render_tests
    PRIVATE
        astrocore_lib
        nlohmann_json::nlohmann_json
        Catch2::Catch2WithMain
)

target_include_directories(render_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

catch_discover_tests(render_tests)
