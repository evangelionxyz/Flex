file(GLOB_RECURSE FLEX_SOURCE
    "Source/App/**.cpp"
    "Source/App/**.hpp"
    "Source/App/**.h"
)
add_executable(Flex ${FLEX_SOURCE})
target_link_libraries(Flex PRIVATE FlexEngine)