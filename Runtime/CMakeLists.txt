file(GLOB SOURCES
    "Source/**.cpp"
    "Source/**.c"
    "Source/**.hpp"
    "Source/**.h"
)

add_executable(Runtime ${SOURCES})
