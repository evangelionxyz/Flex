// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

namespace flex
{
    struct ShaderData
    {
        std::string str; // filepath / source
        GLenum type;
        uint32_t shader = 0;
    };

    class Shader
    {
    public:
        Shader();

        Shader &CreateFromFile(const std::vector<ShaderData> &shaders);
        Shader &CreateFromSource(const std::vector<ShaderData> &shaders);
        Shader &Compile();
        void Reload();

        uint32_t GetProgram() const { return m_Program; }
        void Use();
        static void Use(uint32_t program);

        void SetUniform(std::string_view name, int value);
        void SetUniform(std::string_view name, float value);
        void SetUniform(std::string_view name, const glm::vec3 &vec);
        void SetUniform(std::string_view name, const glm::vec4 &vec);
        void SetUniform(std::string_view name, const glm::mat3 &mat);
        void SetUniform(std::string_view name, const glm::mat4 &mat);

        void SetUniformArray(std::string_view name, int *value, int count);

    private:
        bool CompileShader(ShaderData *shaderData);
        bool CompileShaderFromString(ShaderData *shaderData, const std::string &source);
        int GetUniformLocation(const std::string_view name);

        uint32_t m_Program;
        std::vector<ShaderData> m_Shaders;
        std::unordered_map<std::string_view, int> m_UniformLocations;
    };
}

#endif