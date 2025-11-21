// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef ENGINE_H
#define ENGINE_H

#ifdef ENGINE_EXPORT
    #ifdef _WIN32
        #define FLEX_API __declspec(dllexport)
    #elif __linux__
        #define FLEX_API __attribute__((visibility("default")))
    #endif
#else
    #ifdef _WIN32
        #define FLEX_API __declspec(dllimport)
    #elif __linux__
        #define FLEX_API
    #define FLEX_API
#endif

namespace flex
{

}

#endif