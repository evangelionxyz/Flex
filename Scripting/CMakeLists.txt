file(GLOB_RECURSE FLEX_SCRIPTING_SOURCE
    Source/**.cpp
    Source/**.c
    Source/**.h
    Source/**.hpp
)
add_library(FlexScript SHARED ${FLEX_SCRIPTING_SOURCE})

target_link_libraries(FlexScript PRIVATE FlexEngine)