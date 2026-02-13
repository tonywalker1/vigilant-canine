# Distro detection tests

add_executable(detector_test
    detector_test.cpp
    ../../src/distro/detector.cpp
)

target_link_libraries(detector_test PRIVATE
    GTest::gtest_main
)

target_include_directories(detector_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

include(GoogleTest)
gtest_discover_tests(detector_test)
