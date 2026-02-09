#include "ShaderProgram.h"

#include <iostream>
#include <sstream>
#include <vector>

namespace fsvng {

ShaderProgram::~ShaderProgram() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : program_(other.program_)
    , uniformCache_(std::move(other.uniformCache_))
{
    other.program_ = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        if (program_ != 0) {
            glDeleteProgram(program_);
        }
        program_ = other.program_;
        uniformCache_ = std::move(other.uniformCache_);
        other.program_ = 0;
    }
    return *this;
}

bool ShaderProgram::loadFromFiles(const std::string& vertPath, const std::string& fragPath) {
    // Read vertex shader file
    std::ifstream vertFile(vertPath);
    if (!vertFile.is_open()) {
        std::cerr << "ShaderProgram: Failed to open vertex shader file: " << vertPath << std::endl;
        return false;
    }
    std::ostringstream vertStream;
    vertStream << vertFile.rdbuf();
    std::string vertSrc = vertStream.str();

    // Read fragment shader file
    std::ifstream fragFile(fragPath);
    if (!fragFile.is_open()) {
        std::cerr << "ShaderProgram: Failed to open fragment shader file: " << fragPath << std::endl;
        return false;
    }
    std::ostringstream fragStream;
    fragStream << fragFile.rdbuf();
    std::string fragSrc = fragStream.str();

    return loadFromSource(vertSrc, fragSrc);
}

bool ShaderProgram::loadFromSource(const std::string& vertSrc, const std::string& fragSrc) {
    // Clean up any previously loaded program
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
        uniformCache_.clear();
    }

    GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertSrc);
    if (vertShader == 0) {
        return false;
    }

    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (fragShader == 0) {
        glDeleteShader(vertShader);
        return false;
    }

    // Link program
    program_ = glCreateProgram();
    glAttachShader(program_, vertShader);
    glAttachShader(program_, fragShader);
    glLinkProgram(program_);

    GLint success = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<size_t>(logLength));
        glGetProgramInfoLog(program_, logLength, nullptr, log.data());
        std::cerr << "ShaderProgram: Link error:\n" << log.data() << std::endl;

        glDeleteProgram(program_);
        program_ = 0;
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
        return false;
    }

    // Shaders are linked; they can be detached and deleted
    glDetachShader(program_, vertShader);
    glDetachShader(program_, fragShader);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    return true;
}

void ShaderProgram::use() const {
    glUseProgram(program_);
}

void ShaderProgram::unuse() const {
    glUseProgram(0);
}

void ShaderProgram::setInt(const std::string& name, int value) const {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform1i(loc, value);
    }
}

void ShaderProgram::setFloat(const std::string& name, float value) const {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform1f(loc, value);
    }
}

void ShaderProgram::setVec3(const std::string& name, const glm::vec3& v) const {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform3fv(loc, 1, glm::value_ptr(v));
    }
}

void ShaderProgram::setVec4(const std::string& name, const glm::vec4& v) const {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform4fv(loc, 1, glm::value_ptr(v));
    }
}

void ShaderProgram::setMat4(const std::string& name, const glm::mat4& m) const {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
    }
}

GLuint ShaderProgram::compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<size_t>(logLength));
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());

        const char* typeStr = (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
        std::cerr << "ShaderProgram: " << typeStr << " shader compile error:\n"
                  << log.data() << std::endl;

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

GLint ShaderProgram::getUniformLocation(const std::string& name) const {
    auto it = uniformCache_.find(name);
    if (it != uniformCache_.end()) {
        return it->second;
    }

    GLint location = glGetUniformLocation(program_, name.c_str());
    uniformCache_[name] = location;
    return location;
}

} // namespace fsvng
