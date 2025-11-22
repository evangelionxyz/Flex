file(GLOB_RECURSE FlexAudioEngine
    "Source/Audio/**.cpp"
    "Source/Audio/**.hpp"
    "Source/Audio/**.h"
)

add_library(FlexAudioEngine STATIC ${FlexAudioEngine})

target_include_directories(FlexAudioEngine PUBLIC
    "Source"
    "${THIRDPARTY_DIR}/sdl3/include"
    "${THIRDPARTY_DIR}/glm"
    "${THIRDPARTY_DIR}/json"
    "${THIRDPARTY_DIR}/fmod/include"
)

target_link_libraries(FlexAudioEngine PUBLIC SDL3::SDL3 FlexEngine)

if(WIN32)
    target_link_libraries(FlexAudioEngine PUBLIC
        "${THIRDPARTY_DIR}/fmod/lib/windows/x64/fmod_vc.lib"
    )

    add_custom_command(TARGET FlexAudioEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${THIRDPARTY_DIR}/fmod/lib/windows/x64/fmod.dll" $<TARGET_FILE_DIR:FlexAudioEngine>/fmod.dll
        COMMENT "Copying fmod.dll to output directory"
    )
elseif(UNIX AND NOT APPLE)
    set(FMOD_LIB_DIR "${THIRDPARTY_DIR}/fmod/lib/linux/x86_64")

    target_link_libraries(FlexAudioEngine PUBLIC
        $<$<CONFIG:Debug>:${FMOD_LIB_DIR}/libfmodL.so>
        $<$<NOT:$<CONFIG:Debug>>:${FMOD_LIB_DIR}/libfmod.so>
    )

    add_custom_command(TARGET FlexAudioEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:FlexAudioEngine>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FMOD_LIB_DIR}/libfmod.so
            ${FMOD_LIB_DIR}/libfmod.so.14
            ${FMOD_LIB_DIR}/libfmod.so.14.11
            ${FMOD_LIB_DIR}/libfmodL.so
            ${FMOD_LIB_DIR}/libfmodL.so.14
            ${FMOD_LIB_DIR}/libfmodL.so.14.11
            $<TARGET_FILE_DIR:FlexAudioEngine>
        COMMENT "Copying FMOD shared libraries to output directory"
    )
endif()

add_custom_command(TARGET FlexAudioEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${RESOURCES_DIR} $<TARGET_FILE_DIR:FlexAudioEngine>/Resources
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:FlexAudioEngine>
    COMMENT "Copying SDL3.dll to output directory"
)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    target_compile_definitions(FlexAudioEngine PUBLIC FLEX_DEBUG)
else()
    target_compile_definitions(FlexAudioEngine PUBLIC FLEX_RELEASE NDEBUG)
endif()

if(WIN32)
    target_compile_definitions(FlexAudioEngine PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()