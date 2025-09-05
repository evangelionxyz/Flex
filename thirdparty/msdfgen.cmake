file(GLOB_RECURSE MSDFGEN_SOURCE
    thirdparty/msdfatlasgen/msdfgen/core/*.cpp
    thirdparty/msdfatlasgen/msdfgen/core/*.h
    thirdparty/msdfatlasgen/msdfgen/ext/*.cpp
    thirdparty/msdfatlasgen/msdfgen/ext/*.h
    thirdparty/msdfatlasgen/msdfgen/lib/*.cpp
)

add_library(MSDFGEN STATIC
    ${MSDFGEN_SOURCE}
)

target_include_directories(MSDFGEN PRIVATE
    thirdparty/msdfatlasgen/msdfgen/
    thirdparty/msdfatlasgen/msdfgen/include
    thirdparty/msdfatlasgen/msdfgen/freetype/include
)

target_compile_definitions(MSDFGEN PUBLIC
    MSDF_USE_CPP11
)

set_target_properties(MSDFGEN PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
