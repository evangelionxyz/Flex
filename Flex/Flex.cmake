file(GLOB_RECURSE FLEX_SOURCE
    "Source/App/**.cpp"
    "Source/App/**.hpp"
    "Source/App/**.h"
)
add_executable(Flex ${FLEX_SOURCE})
target_include_directories(Flex PRIVATE
    "${THIRDPARTY_DIR}/igniteserver/Networking/source"
    "${THIRDPARTY_DIR}/igniteserver/ThirdParty/GameNetworkingSockets/include"
)
target_link_libraries(Flex PRIVATE FlexEngine FlexAudioEngine IgniteNet)

if(WIN32)
    target_link_libraries(FlexEngine PUBLIC
        "${THIRDPARTY_DIR}/fmod/lib/windows/x64/fmod_vc.lib"
    )
endif()