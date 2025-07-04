#include <glad/glad.h>
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

struct ShaderData
{
    std::string filepath;
    uint32_t shader = 0;
    GLenum type;
};

class Shader
{
public:
    Shader();

    Shader &AddFromFile(const std::string &filepath, GLenum type);
    Shader &Compile();
    void Reload();

    uint32_t GetProgram() const { return m_Program; }
    void Use();
    static void Use(uint32_t program);

    void SetUniform(std::string_view name, int value);
    void SetUniform(std::string_view name, const glm::vec3 &vec);
    void SetUniform(std::string_view name, const glm::vec4 &vec);
    void SetUniform(std::string_view name, const glm::mat3 &mat);
    void SetUniform(std::string_view name, const glm::mat4 &mat);

    void SetUniformArray(std::string_view name, int *value, int count);

private:
    bool CompileShader(ShaderData *shaderData);
    int GetUniformLocation(const std::string_view name);

    uint32_t m_Program;
    std::vector<ShaderData> m_Shaders;
    std::unordered_map<std::string_view, int> m_UniformLocations;
};

