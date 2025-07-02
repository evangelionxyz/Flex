#include <glad/glad.h>
#include <cstdint>
#include <vector>
#include <string>

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

private:
    bool CompileShader(ShaderData *shaderData);

    uint32_t m_Program;
    std::vector<ShaderData> m_Shaders;
};

