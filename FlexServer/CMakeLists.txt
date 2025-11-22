file(GLOB_RECURSE FLEX_SERVER_SOURCES
    "Source/**.cpp"
    "Source/**.hpp"
    "Source/**.h"
)

add_executable(FlexServer ${FLEX_SERVER_SOURCES})

target_include_directories(FlexServer PRIVATE
    "${CMAKE_SOURCE_DIR}/Flex/Source"
    "${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/Networking/source"
    "${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/ThirdParty/GameNetworkingSockets/include"
)

target_link_libraries(FlexServer PRIVATE FlexEngine FlexAudioEngine IgniteNet)

if(WIN32)
    # FMOD 
    target_link_libraries(FlexServer PUBLIC "${THIRDPARTY_DIR}/fmod/lib/windows/x64/fmod_vc.lib")

    add_custom_command(TARGET FlexServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${THIRDPARTY_DIR}/fmod/lib/windows/x64/fmod.dll" $<TARGET_FILE_DIR:FlexServer>/fmod.dll
        COMMENT "Copying fmod.dll to output directory"
    )

    # GameNetworkingSocets
    target_link_libraries(FlexServer PUBLIC
        $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/ThirdParty/GameNetworkingSockets/lib/Windows/Debug/GameNetworkingSockets.lib>
        $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/ThirdParty/GameNetworkingSockets/lib/Windows/Release/GameNetworkingSockets.lib>
        $<$<CONFIG:RelWithDebInfo>:${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/ThirdParty/GameNetworkingSockets/lib/Windows/Release/GameNetworkingSockets.lib>
        $<$<CONFIG:MinSizeRel>:${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/ThirdParty/GameNetworkingSockets/lib/Windows/Release/GameNetworkingSockets.lib>
    )

    add_custom_command(TARGET FlexServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/ThirdParty/GameNetworkingSockets/lib/Windows/$<CONFIG>/GameNetworkingSockets.dll"
        "$<TARGET_FILE_DIR:FlexServer>/GameNetworkingSockets.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/ThirdParty/GameNetworkingSockets/lib/Windows/$<CONFIG>/libcrypto-3-x64.dll"
        "$<TARGET_FILE_DIR:FlexServer>/libcrypto-3-x64.dll"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/ThirdParty/GameNetworkingSockets/lib/Windows/$<CONFIG>/libprotobuf$<$<CONFIG:Debug>:d>.dll"
        "$<TARGET_FILE_DIR:FlexServer>/libprotobuf$<$<CONFIG:Debug>:d>.dll"
    )
elseif(UNIX AND NOT APPLE)
    # FMOD
    set(FMOD_LIB_DIR "${THIRDPARTY_DIR}/fmod/lib/linux/x86_64")
    target_link_libraries(FlexServer PUBLIC
        $<$<CONFIG:Debug>:${FMOD_LIB_DIR}/libfmodL.so>
        $<$<NOT:$<CONFIG:Debug>>:${FMOD_LIB_DIR}/libfmod.so>
    )

    # GameNetworkingSockets
    target_link_libraries(FlexServer PUBLIC
        ${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/ThirdParty/GameNetworkingSockets/lib/Linux/libGameNetworkingSockets.so
        ${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/ThirdParty/GameNetworkingSockets/lib/Linux/libprotobuf.so.23
    )

    add_custom_command(TARGET FlexServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/ThirdParty/GameNetworkingSockets/lib/Linux/libGameNetworkingSockets.so"
        "$<TARGET_FILE_DIR:FlexServer>/libGameNetworkingSockets.so"

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/thirdparty/igniteserver/ThirdParty/GameNetworkingSockets/lib/Linux/libprotobuf.so.23"
        "$<TARGET_FILE_DIR:FlexServer>/libprotobuf.so.23"
    )
endif()

add_custom_command(TARGET FlexServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:FlexServer>
    COMMENT "Copying SDL3.dll to output directory"
)