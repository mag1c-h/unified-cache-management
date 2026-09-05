include(GoogleTest)
file(GLOB_RECURSE UCM_V2_TEST_SOURCE_FILES CONFIGURE_DEPENDS "./ut/*.cc")
add_executable(ucm_v2.test ${UCM_V2_TEST_SOURCE_FILES})
target_link_libraries(ucm_v2.test PRIVATE
    detail
    gtest_main
    gtest
)
gtest_discover_tests(ucm_v2.test)
