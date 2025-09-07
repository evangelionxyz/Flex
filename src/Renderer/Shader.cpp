#include "Shader.h"

#include <sstream>
#include <fstream>
#include <iostream>
#include <cassert>
#include <filesystem>

#include <glm/gtc/type_ptr.hpp>

static const char *GetShaderStageString(GLenum stage)
{
    switch(stage)
    {
        case GL_VERTEX_SHADER: return "VERTEX_SHADER";
        case GL_FRAGMENT_SHADER: return "FRAGMENT_SHADER";
        case GL_COMPUTE_SHADER: return "COMPUTE_SHADER";
        default: return "INVALID_SHADER";
    }
}

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
    else
    {
        assert(false);
    }

    return *this;
}

Shader &Shader::AddFromString(const std::string &source, GLenum type)
{
    ShaderData shaderData;
    shaderData.type = type;
    shaderData.filepath = ""; // No filepath for string shaders
    
    if (CompileShaderFromString(&shaderData, source))
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
    assert(std::filesystem::exists(shaderData->filepath) && "Shader file does not exists!");

    std::ifstream shaderFile(shaderData->filepath);
	std::stringstream stream;
	stream << shaderFile.rdbuf();
	std::string sourceStr = stream.str();
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
		std::cerr << "Failed to compile " << GetShaderStageString(shaderData->type) << " \"" << shaderData->filepath <<"\"\n";

		// Get shader info log
		int logSize = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
		std::vector<char> messageLog(logSize);
		glGetShaderInfoLog(shader, logSize, &logSize, messageLog.data());
		std::cerr << messageLog.data() << '\n';;

		glDeleteShader(shader);

		return false;
	}

    shaderData->shader = shader;
    
    std::cout << "Shader " << GetShaderStageString(shaderData->type) << " Success to compile: \"" << shaderData->filepath << '\n';
    return true;
}

bool Shader::CompileShaderFromString(ShaderData *shaderData, const std::string &source)
{
    const char *shaderCode = source.c_str();

    uint32_t shader = glCreateShader(shaderData->type);
    glShaderSource(shader, 1, &shaderCode, nullptr);

	// Compile shader and get compile status
	glCompileShader(shader);
	int status = GL_FALSE;

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

	// Failed to compile shader
	if (status == GL_FALSE)
	{
		std::cerr << "Failed to compile " << GetShaderStageString(shaderData->type) << " from string\n";

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
    
    std::cout << "Shader " << GetShaderStageString(shaderData->type) << " Success to compile from string\n";
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

void Shader::SetUniform(std::string_view name, float value)
{
    int location = GetUniformLocation(name);
    if (location != -1)
    {
        glUniform1f(location, value);
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