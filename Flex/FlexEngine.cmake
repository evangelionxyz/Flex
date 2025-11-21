file(GLOB_RECURSE FLEX_ENGINE_SOURCES
    "Source/Core/**.cpp"
    "Source/Core/**.hpp"
    "Source/Core/**.h"

    "Source/Math/**.cpp"
    "Source/Math/**.hpp"
    "Source/Math/**.h"

    "Source/Physics/**.cpp"
    "Source/Physics/**.hpp"
    "Source/Physics/**.h"

    "Source/Renderer/**.cpp"
    "Source/Renderer/**.hpp"
    "Source/Renderer/**.h"

    "Source/Scene/**.cpp"
    "Source/Scene/**.hpp"
    "Source/Scene/**.h"
)

add_library(FlexEngine STATIC
    ${FLEX_ENGINE_SOURCES}
    "${THIRDPARTY_DIR}/tinygltf/tinygltf.cpp"
    "${THIRDPARTY_DIR}/stb/stb.c"
    "${THIRDPARTY_DIR}/glad/src/glad.c"
)

target_include_directories(FlexEngine PUBLIC
    "Source"
    "${THIRDPARTY_DIR}/sdl3/include"
    "${THIRDPARTY_DIR}/stb"
    "${THIRDPARTY_DIR}/glm"
    "${THIRDPARTY_DIR}/jolt"
    "${THIRDPARTY_DIR}/json"
    "${THIRDPARTY_DIR}/imguizmo"
    "${THIRDPARTY_DIR}/entt/single_include"
    "${THIRDPARTY_DIR}/tinygltf/include"
)

target_link_libraries(FlexEngine PUBLIC SDL3::SDL3 Jolt imgui MSDF_ATLAS_GEN MSDFGEN FREETYPE)
add_custom_command(TARGET FlexEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${RESOURCES_DIR} $<TARGET_FILE_DIR:FlexEngine>/Resources
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:FlexEngine>
    COMMENT "Copying SDL3.dll to output directory"
)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(FlexEngine PUBLIC FLEX_DEBUG)
else()
    target_compile_definitions(FlexEngine PUBLIC FLEX_RELEASE NDEBUG)
endif()

if(WIN32)
    target_compile_definitions(FlexEngine PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()