#include "render/SphereMesh.hpp"
#include <cmath>
#include <glm/glm.hpp>

namespace astrocore {

SphereMesh::~SphereMesh() {
    destroy();
}

void SphereMesh::create(int latSegments, int lonSegments) {
    destroy();

    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    const float PI = 3.14159265359f;

    // Generate vertices
    for (int lat = 0; lat <= latSegments; lat++) {
        float theta = lat * PI / latSegments;
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (int lon = 0; lon <= lonSegments; lon++) {
            float phi = lon * 2.0f * PI / lonSegments;
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            // Position (normalized, radius = 1)
            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            // Normal (same as position for unit sphere)
            float nx = x;
            float ny = y;
            float nz = z;

            // Position
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // Normal
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);
        }
    }

    // Generate indices
    for (int lat = 0; lat < latSegments; lat++) {
        for (int lon = 0; lon < lonSegments; lon++) {
            int first = lat * (lonSegments + 1) + lon;
            int second = first + lonSegments + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    m_indexCount = static_cast<int>(indices.size());

    // Create VAO
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    // VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Normal attribute (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void SphereMesh::draw() const {
    if (!m_vao) return;

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void SphereMesh::destroy() {
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_ebo) {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }
    m_indexCount = 0;
}

}  // namespace astrocore
