# Core tests

add_executable(hash_test
    hash_test.cpp
    ../../src/core/hash.cpp
)

target_link_libraries(hash_test PRIVATE
    GTest::gtest_main
    ${OPENSSL_LIBRARIES}
    ${BLAKE3_LIBRARIES}
)

if(HINDER_FOUND)
    target_link_libraries(hash_test PRIVATE hinder::hinder)
endif()

target_include_directories(hash_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${OPENSSL_INCLUDE_DIRS}
    ${BLAKE3_INCLUDE_DIRS}
)

include(GoogleTest)
gtest_discover_tests(hash_test)
