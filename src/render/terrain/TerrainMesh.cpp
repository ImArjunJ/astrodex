#include "render/terrain/TerrainMesh.hpp"
#include "core/Logger.hpp"

namespace astrocore {

TerrainMesh::TerrainMesh() {
    m_instances.reserve(MAX_INSTANCES);
}

TerrainMesh::~TerrainMesh() {
    destroy();
}

void TerrainMesh::init() {
    createPatchMesh();
    createInstanceBuffer();
    LOG_INFO("TerrainMesh initialized: {}x{} grid, max {} instances",
             GRID_SIZE, GRID_SIZE, MAX_INSTANCES);
}

void TerrainMesh::destroy() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_instanceVBO) glDeleteBuffers(1, &m_instanceVBO);
    m_vao = m_vbo = m_ebo = m_instanceVBO = 0;
}

void TerrainMesh::createPatchMesh() {
    // Create a single patch mesh that will be instanced
    // Vertices are in [0,1] range, transformed per-instance in shader

    std::vector<glm::vec2> vertices;
    std::vector<unsigned int> indices;

    // Generate grid vertices
    for (int y = 0; y <= GRID_SIZE; ++y) {
        for (int x = 0; x <= GRID_SIZE; ++x) {
            float u = static_cast<float>(x) / GRID_SIZE;
            float v = static_cast<float>(y) / GRID_SIZE;
            vertices.emplace_back(u, v);
        }
    }

    // Generate indices for quad patches (4 vertices per patch for tessellation)
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            unsigned int bl = static_cast<unsigned int>(y * (GRID_SIZE + 1) + x);
            unsigned int br = bl + 1;
            unsigned int tl = bl + (GRID_SIZE + 1);
            unsigned int tr = tl + 1;

            indices.push_back(bl);
            indices.push_back(br);
            indices.push_back(tr);
            indices.push_back(tl);
        }
    }

    m_indexCount = static_cast<int>(indices.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    // Vertex buffer (just UV coords)
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec2)),
                 vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), nullptr);

    // Index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void TerrainMesh::createInstanceBuffer() {
    glGenBuffers(1, &m_instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 MAX_INSTANCES * sizeof(InstanceData),
                 nullptr, GL_DYNAMIC_DRAW);

    // Bind instance attributes to VAO
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);

    // Instance attribute: uvBounds (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          reinterpret_cast<void*>(offsetof(InstanceData, uvBounds)));
    glVertexAttribDivisor(1, 1);

    // Instance attribute: face (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          reinterpret_cast<void*>(offsetof(InstanceData, face)));
    glVertexAttribDivisor(2, 1);

    // Instance attribute: level (location 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          reinterpret_cast<void*>(offsetof(InstanceData, level)));
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
}

void TerrainMesh::beginFrame() {
    m_instances.clear();
    m_instanceCount = 0;
}

bool TerrainMesh::addInstance(int face, const glm::vec2& minUV, const glm::vec2& maxUV, int level) {
    if (m_instanceCount >= MAX_INSTANCES) {
        return false;
    }

    InstanceData data;
    data.uvBounds = glm::vec4(minUV.x, minUV.y, maxUV.x, maxUV.y);
    data.face = static_cast<float>(face);
    data.level = static_cast<float>(level);
    data._pad[0] = 0.0f;
    data._pad[1] = 0.0f;

    m_instances.push_back(data);
    m_instanceCount++;
    return true;
}

void TerrainMesh::render() {
    if (m_instanceCount == 0) return;

    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(m_instanceCount * sizeof(InstanceData)),
                    m_instances.data());

    // Render all patches with one instanced draw call
    glBindVertexArray(m_vao);
    glPatchParameteri(GL_PATCH_VERTICES, 4);
    glDrawElementsInstanced(GL_PATCHES, m_indexCount, GL_UNSIGNED_INT, nullptr, m_instanceCount);
    glBindVertexArray(0);
}

}  // namespace astrocore
