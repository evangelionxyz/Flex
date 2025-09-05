add_library(FREETYPE
    thirdparty/msdfatlasgen/msdfgen/freetype/include/ft2build.h
    thirdparty/msdfatlasgen/msdfgen/freetype/src/autofit/autofit.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftbase.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftbbox.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftbdf.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftbitmap.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftcid.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftdebug.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftfstype.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftgasp.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftglyph.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftgxval.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftinit.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftmm.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftotval.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftpatent.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftpfr.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftstroke.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftsynth.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftsystem.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/fttype1.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/base/ftwinfnt.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/bdf/bdf.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/bzip2/ftbzip2.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/cache/ftcache.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/cff/cff.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/cid/type1cid.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/gzip/ftgzip.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/lzw/ftlzw.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/pcf/pcf.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/pfr/pfr.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/psaux/psaux.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/pshinter/pshinter.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/psnames/psnames.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/raster/raster.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/sdf/sdf.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/svg/svg.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/sfnt/sfnt.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/smooth/smooth.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/truetype/truetype.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/type1/type1.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/type42/type42.c
    thirdparty/msdfatlasgen/msdfgen/freetype/src/winfonts/winfnt.c
)

target_compile_definitions(FREETYPE PRIVATE
    -DFT2_BUILD_LIBRARY
)

target_include_directories(FREETYPE PUBLIC
    thirdparty/msdfatlasgen/msdfgen/freetype/include
)

set_target_properties(FREETYPE PROPERTIES C_STANDARD 17 CXX_STANDARD_REQUIRED YES)
