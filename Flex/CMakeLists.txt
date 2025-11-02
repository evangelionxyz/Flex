file(GLOB_RECURSE FLEX_SOURCE_FILES
    "Source/**.c"
    "Source/**.h"
    "Source/**.cpp"
    "Source/**.hpp"
)

add_executable(FlexEngine
    ${FLEX_SOURCE_FILES}
    "${THIRDPARTY_DIR}/tinygltf/tinygltf.cpp"
    "${THIRDPARTY_DIR}/stb/stb.c"
    "${THIRDPARTY_DIR}/glad/src/glad.c"
)

target_include_directories(FlexEngine PRIVATE
    "Source"
    "${THIRDPARTY_DIR}/sdl3/include"
    "${THIRDPARTY_DIR}/stb"
    "${THIRDPARTY_DIR}/glm"
    "${THIRDPARTY_DIR}/json"
    "${THIRDPARTY_DIR}/tinygltf/include"
)

target_link_libraries(FlexEngine PRIVATE SDL3::SDL3 imgui MSDF_ATLAS_GEN MSDFGEN FREETYPE)

add_custom_command(TARGET FlexEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${RESOURCES_DIR} $<TARGET_FILE_DIR:FlexEngine>/Resources
)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(FlexEngine PRIVATE FLEX_DEBUG)
else()
    target_compile_definitions(FlexEngine PRIVATE FLEX_RELEASE NDEBUG)
endif()

if(WIN32)
    target_compile_definitions(FlexEngine PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()


