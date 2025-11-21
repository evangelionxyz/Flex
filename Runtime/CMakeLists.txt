file(GLOB_RECURSE FLEX_RUNTIME_SOURCES
    "Source/**.cpp"
    "Source/**.c"
    "Source/**.hpp"
    "Source/**.h"
)

add_executable(FlexRuntime ${FLEX_RUNTIME_SOURCES})
target_link_libraries(FlexRuntime PRIVATE FlexEngine)
