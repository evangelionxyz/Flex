add_subdirectory("${THIRDPARTY_DIR}/googletest" "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)

file(GLOB_RECURSE FLEX_TEST_SOURCES
    "Source/**.c"
    "Source/**.h"
    "Source/**.cpp"
    "Source/**.hpp"
)

add_executable(FlexTest ${FLEX_TEST_SOURCES})

target_link_libraries(FlexTest PRIVATE FlexEngineLib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(FlexTest)
    