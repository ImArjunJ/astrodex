#pragma once

#include <glad/gl.h>
#include <vector>

namespace astrocore {

class SphereMesh {
public:
    SphereMesh() = default;
    ~SphereMesh();

    // Create a UV sphere with given subdivisions
    void create(int latSegments = 64, int lonSegments = 64);

    // Render the sphere
    void draw() const;

    // Cleanup
    void destroy();

    bool isValid() const { return m_vao != 0; }

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    int m_indexCount = 0;
};

}  // namespace astrocore
