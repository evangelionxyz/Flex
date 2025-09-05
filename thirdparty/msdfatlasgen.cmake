file(GLOB_RECURSE MSDF_ATLAS_GEN_SOURCE 
    thirdparty/msdfatlasgen/msdf-atlas-gen/*.cpp
    thirdparty/msdfatlasgen/msdf-atlas-gen/*.h
)
add_library(MSDF_ATLAS_GEN
    ${MSDF_ATLAS_GEN_SOURCE}
)

target_include_directories(MSDF_ATLAS_GEN PUBLIC
    thirdparty/msdfatlasgen/msdf-atlas-gen/
    thirdparty/msdfatlasgen/msdfgen/
    thirdparty/msdfatlasgen/msdfgen/include/
)

set_target_properties(MSDF_ATLAS_GEN PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)
