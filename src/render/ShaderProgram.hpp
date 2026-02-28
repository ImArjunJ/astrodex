#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

namespace astrocore {

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    // Prevent copying
    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    // Move operations
    ShaderProgram(ShaderProgram&& other) noexcept;
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;

    // Load shaders from files
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    bool loadFromSource(const std::string& vertexSource, const std::string& fragmentSource);

    // Use this shader program
    void use() const;
    void unuse() const;

    // Check if program is valid
    bool isValid() const { return m_program != 0; }
    GLuint getHandle() const { return m_program; }

    // Uniform setters
    void setBool(const std::string& name, bool value);
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec4(const std::string& name, const glm::vec4& value);
    void setMat3(const std::string& name, const glm::mat3& value);
    void setMat4(const std::string& name, const glm::mat4& value);

    // Reload shaders (hot reload support)
    bool reload();

private:
    GLuint compileShader(GLenum type, const std::string& source);
    bool linkProgram(GLuint vertexShader, GLuint fragmentShader);
    GLint getUniformLocation(const std::string& name);
    std::string readFile(const std::string& path);

    GLuint m_program = 0;
    std::unordered_map<std::string, GLint> m_uniformCache;

    // Store paths for hot reload
    std::string m_vertexPath;
    std::string m_fragmentPath;
};

}  // namespace astrocore
