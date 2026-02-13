# Package verifier tests

add_executable(package_verifier_test
    package_verifier_test.cpp
    ../../src/package/package_verifier.cpp
)

target_link_libraries(package_verifier_test PRIVATE
    GTest::gtest_main
)

if(HINDER_FOUND)
    target_link_libraries(package_verifier_test PRIVATE hinder::hinder)
endif()

target_include_directories(package_verifier_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

include(GoogleTest)
gtest_discover_tests(package_verifier_test)
