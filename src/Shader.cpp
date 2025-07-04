#include "Shader.h"

#include <sstream>
#include <fstream>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

Shader::Shader()
    : m_Program(0)
{
}

Shader &Shader::AddFromFile(const std::string &filepath, GLenum type)
{
    ShaderData shaderData;
    shaderData.type = type;
    shaderData.filepath = filepath;
    
    if (CompileShader(&shaderData))
    {
        m_Shaders.push_back(std::move(shaderData));
    } 

    return *this;
}

Shader &Shader::Compile()
{
    GLuint program = glCreateProgram();

    for (auto &[filepath, shader, type] : m_Shaders)
    {
	    glAttachShader(program, shader);
    }

	glLinkProgram(program);

	int status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &status);

	if (status == GL_FALSE)
	{
		std::cerr << "Failed to link shader program\n";

		int logSize = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);
		std::vector<char> messageLog(logSize);
		glGetProgramInfoLog(program, logSize, &logSize, messageLog.data());
		std::cerr << messageLog.data() << '\n';;

		glDeleteProgram(program);

		std::exit(EXIT_FAILURE);
	}

    for (auto &[filepath, shader, type] : m_Shaders)
    {
	    glDeleteShader(shader);
    }

    m_Program = program;

    return *this;
}

bool Shader::CompileShader(ShaderData *shaderData)
{
    std::ifstream shaderFile(shaderData->filepath);
	std::stringstream stream;
	stream << shaderFile.rdbuf();
	std::string sourceStr = stream.str();
	std::cout << "=== Shader Source ===\n";
	std::cout << sourceStr << "\n";
	std::cout << "=========\n";
	const char *shaderCode = sourceStr.c_str();

    uint32_t shader = glCreateShader(shaderData->type);
    glShaderSource(shader, 1, &shaderCode, nullptr);

	// Compile shader and get compile status
	glCompileShader(shader);
	int status = GL_FALSE;

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

	// Failed to compile shader
	if (status == GL_FALSE)
	{
		std::cerr << "Failed to compile " << ((shaderData->type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT") << " shader \n";

		// Get shader info log
		int logSize = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
		std::vector<char> messageLog(logSize);
		glGetShaderInfoLog(shader, logSize, &logSize, messageLog.data());
		std::cerr << messageLog.data() << '\n';;

		glDeleteShader(shader);

        assert(false);
		return false;
	}

    shaderData->shader = shader;

    return true;
}

void Shader::Reload()
{
    bool success = true;
    for (auto &[filepath, shader, type] : m_Shaders)
    {
        ShaderData shaderData;
        shaderData.type = type;
        shaderData.filepath = filepath;
        
        if (!CompileShader(&shaderData))
        {
            success = false;
            break;
        }

        shader = shaderData.shader;
    }

    if (success)
    {
        Compile();
    }
}

void Shader::Use()
{
    glUseProgram(m_Program);
}

void Shader::Use(uint32_t program)
{
    if (glIsProgram(program))
        glUseProgram(program);
}

void Shader::SetUniform(std::string_view name, int value)
{
    int location = GetUniformLocation(name);
    if (location != -1)
    {
        glUniform1i(location, value);
    }
}

void Shader::SetUniform(std::string_view name, const glm::vec3 &vec)
{
    int location = GetUniformLocation(name);
    if (location != -1)
    {
        glUniform3f(location, vec.x, vec.y, vec.z);
    }
}

void Shader::SetUniform(std::string_view name, const glm::vec4 &vec)
{
    int location = GetUniformLocation(name);
    if (location != -1)
    {
        glUniform4f(location, vec.x, vec.y, vec.z, vec.w);
    }
}

void Shader::SetUniform(std::string_view name, const glm::mat3 &mat)
{
    int location = GetUniformLocation(name);
    if (location != -1)
    {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(mat));
    }
}

void Shader::SetUniform(std::string_view name, const glm::mat4 &mat)
{
    int location = GetUniformLocation(name);
    if (location != -1)
    {
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(mat));
    }
}

void Shader::SetUniformArray(std::string_view name, int *value, int count)
{
    int location = GetUniformLocation(name);
    if (location != -1)
    {
        glUniform1iv(location, count, value);
    }
}

int Shader::GetUniformLocation(const std::string_view name)
{
    auto it = m_UniformLocations.find(name);
    if (it != m_UniformLocations.end())
        return it->second;

    int location = glGetUniformLocation(m_Program, name.data());
    if (location == -1)
    {
        std::cerr << "Warning: Uniform '" << name << "' not found in shader program.\n";
    }
    
    m_UniformLocations[name] = location;
    return location;
}