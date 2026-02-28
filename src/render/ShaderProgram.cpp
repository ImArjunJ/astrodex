#include "render/ShaderProgram.hpp"
#include "core/Logger.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>

namespace astrocore {

ShaderProgram::~ShaderProgram() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
    }
}

ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
    : m_program(other.m_program)
    , m_uniformCache(std::move(other.m_uniformCache))
    , m_vertexPath(std::move(other.m_vertexPath))
    , m_fragmentPath(std::move(other.m_fragmentPath))
{
    other.m_program = 0;
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
    if (this != &other) {
        if (m_program != 0) {
            glDeleteProgram(m_program);
        }
        m_program = other.m_program;
        m_uniformCache = std::move(other.m_uniformCache);
        m_vertexPath = std::move(other.m_vertexPath);
        m_fragmentPath = std::move(other.m_fragmentPath);
        other.m_program = 0;
    }
    return *this;
}

std::string ShaderProgram::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open shader file: {}", path);
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool ShaderProgram::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    m_vertexPath = vertexPath;
    m_fragmentPath = fragmentPath;

    std::string vertexSource = readFile(vertexPath);
    std::string fragmentSource = readFile(fragmentPath);

    if (vertexSource.empty() || fragmentSource.empty()) {
        return false;
    }

    return loadFromSource(vertexSource, fragmentSource);
}

bool ShaderProgram::loadFromSource(const std::string& vertexSource, const std::string& fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (vertexShader == 0) {
        return false;
    }

    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    bool success = linkProgram(vertexShader, fragmentShader);

    // Shaders can be deleted after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return success;
}

GLuint ShaderProgram::compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        LOG_ERROR("Shader compilation failed ({}): {}",
            type == GL_VERTEX_SHADER ? "vertex" : "fragment", infoLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool ShaderProgram::linkProgram(GLuint vertexShader, GLuint fragmentShader) {
    if (m_program != 0) {
        glDeleteProgram(m_program);
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);
    glLinkProgram(m_program);

    GLint success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(m_program, sizeof(infoLog), nullptr, infoLog);
        LOG_ERROR("Shader program linking failed: {}", infoLog);
        glDeleteProgram(m_program);
        m_program = 0;
        return false;
    }

    // Clear uniform cache on reload
    m_uniformCache.clear();
    return true;
}

bool ShaderProgram::reload() {
    if (m_vertexPath.empty() || m_fragmentPath.empty()) {
        LOG_WARN("Cannot reload shader: no source files specified");
        return false;
    }
    LOG_INFO("Reloading shader: {} + {}", m_vertexPath, m_fragmentPath);
    return loadFromFiles(m_vertexPath, m_fragmentPath);
}

void ShaderProgram::use() const {
    glUseProgram(m_program);
}

void ShaderProgram::unuse() const {
    glUseProgram(0);
}

GLint ShaderProgram::getUniformLocation(const std::string& name) {
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) {
        return it->second;
    }

    GLint location = glGetUniformLocation(m_program, name.c_str());
    if (location == -1) {
        LOG_WARN("Uniform '{}' not found in shader", name);
    }
    m_uniformCache[name] = location;
    return location;
}

void ShaderProgram::setBool(const std::string& name, bool value) {
    glUniform1i(getUniformLocation(name), static_cast<int>(value));
}

void ShaderProgram::setInt(const std::string& name, int value) {
    glUniform1i(getUniformLocation(name), value);
}

void ShaderProgram::setFloat(const std::string& name, float value) {
    glUniform1f(getUniformLocation(name), value);
}

void ShaderProgram::setVec2(const std::string& name, const glm::vec2& value) {
    glUniform2fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void ShaderProgram::setVec3(const std::string& name, const glm::vec3& value) {
    glUniform3fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void ShaderProgram::setVec4(const std::string& name, const glm::vec4& value) {
    glUniform4fv(getUniformLocation(name), 1, glm::value_ptr(value));
}

void ShaderProgram::setMat3(const std::string& name, const glm::mat3& value) {
    glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void ShaderProgram::setMat4(const std::string& name, const glm::mat4& value) {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

}  // namespace astrocore
