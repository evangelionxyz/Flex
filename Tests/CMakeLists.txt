add_subdirectory("${THIRDPARTY_DIR}/googletest" "${CMAKE_CURRENT_BINARY_DIR}/googletest" EXCLUDE_FROM_ALL)

file(GLOB_RECURSE FLEX_TEST_SOURCES
    "Source/**.c"
    "Source/**.h"
    "Source/**.cpp"
    "Source/**.hpp"
)

add_executable(FlexTest ${FLEX_TEST_SOURCES})
target_link_libraries(FlexTest PRIVATE FlexEngineLib GTest::gtest_main)

add_custom_command(TARGET FlexTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:FlexTest>
    COMMENT "Copying SDL3.dll for FlexTest"
)

include(GoogleTest)
gtest_discover_tests(FlexTest)