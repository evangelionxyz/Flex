#pragma once

#include <glad/glad.h>
#include <cassert>

enum class RasterizeFillMode
{
    SOLID,
    WIREFRAME,
};

enum class CullMode
{
    FRONT,
    BACK,
    NONE
};

enum class ComparisonFunc
{
    EQUAL,
    LESS_OR_EQUAL,
    GREATER_OR_EQUAL,
};

enum class ClampMode
{
    REPEAT,
    CLAMP_TO_BORDER,
    CLAMP_TO_EDGE,
};

enum class FilterMode
{
    LINEAR,
    NEAREST,
    LINEAR_MIPMAP_LINEAR,
    LINEAR_MIPMAP_NEAREST,
};

enum class Format
{
    R8,
    RGB8,
    RGB16F,
    RGB32F,

    RGBA8,
    RGBA16F,
    RGBA32F,

    DEPTH24STENCIL8,
};

static GLenum ToGLRasterizeFillMode(RasterizeFillMode mode)
{
    switch (mode)
    {
        case RasterizeFillMode::SOLID: return GL_FILL;
        case RasterizeFillMode::WIREFRAME: return GL_LINE;
        default: return GL_FILL;
    }
}

static GLenum ToGLCullMode(CullMode mode)
{
    switch (mode)
    {
        case CullMode::FRONT: return GL_FRONT;
        case CullMode::BACK: return GL_BACK;
        case CullMode::NONE: return GL_FRONT_AND_BACK;
        default: return GL_FRONT_AND_BACK;
    }
}

static GLenum ToGLClampMode(ClampMode clamp)
{
    switch (clamp)
    {
        case ClampMode::CLAMP_TO_BORDER: return GL_CLAMP_TO_BORDER;
        case ClampMode::CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
        case ClampMode::REPEAT: return GL_REPEAT;
        default:
        return GL_REPEAT;
    }
}

static GLenum ToGLFilter(FilterMode filter)
{
    switch (filter)
    {
        case FilterMode::LINEAR: return GL_LINEAR;
        case FilterMode::NEAREST: return GL_NEAREST;
        case FilterMode::LINEAR_MIPMAP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
        case FilterMode::LINEAR_MIPMAP_NEAREST: return GL_LINEAR_MIPMAP_NEAREST;
        default:
        return GL_LINEAR;
    }
}

static GLenum ToGLFormat(Format format)
{
    switch (format)
    {
        case Format::R8: return GL_RED;
        case Format::RGB8: return GL_RGB;
        case Format::RGB16F: return GL_RGB;
        case Format::RGB32F: return GL_RGB;
        
        case Format::RGBA8: return GL_RGBA;
        case Format::RGBA16F: return GL_RGBA;
        case Format::RGBA32F: return GL_RGBA;
        
        case Format::DEPTH24STENCIL8: return GL_DEPTH_STENCIL;
        default:
            assert(false);
            return GL_INVALID_ENUM;
        break;
    }
}

static GLenum ToGLInternalFormat(Format format)
{
    switch (format)
    {
        case Format::R8: return GL_R8;
        case Format::RGB8: return GL_RGB8;
        case Format::RGB16F: return GL_RGB16F;
        case Format::RGB32F: return GL_RGB32F;

        case Format::RGBA8: return GL_RGBA8;
        case Format::RGBA16F: return GL_RGBA16F;
        case Format::RGBA32F: return GL_RGBA32F;
        
        case Format::DEPTH24STENCIL8: return GL_DEPTH24_STENCIL8;
        default:
            assert(false);
            return GL_INVALID_ENUM;
        break;
    }
}