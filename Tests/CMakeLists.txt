file(GLOB_RECURSE FLEX_TEST_SOURCES
    "Source/**.c"
    "Source/**.h"
    "Source/**.cpp"
    "Source/**.hpp"
)

add_executable(FlexTest ${FLEX_TEST_SOURCES})
    