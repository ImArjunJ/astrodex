#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>

namespace astrocore {

// Shared patch mesh - single VAO/VBO reused by all terrain nodes via instancing
// This eliminates per-node GL object creation which was killing performance
class TerrainMesh {
public:
    static constexpr int GRID_SIZE = 16;  // 16x16 vertices per patch
    static constexpr int MAX_INSTANCES = 4096;  // Max visible patches

    TerrainMesh();
    ~TerrainMesh();

    void init();
    void destroy();

    // Clear instance data for new frame
    void beginFrame();

    // Add a patch instance to render this frame
    // Returns false if instance buffer is full
    bool addInstance(int face, const glm::vec2& minUV, const glm::vec2& maxUV, int level);

    // Render all instances in one draw call
    void render();

    // Get instance count for stats
    int getInstanceCount() const { return m_instanceCount; }

private:
    void createPatchMesh();
    void createInstanceBuffer();

    // Shared patch geometry
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ebo = 0;
    int m_indexCount = 0;

    // Instance data buffer (face, minUV, maxUV, level)
    GLuint m_instanceVBO = 0;

    struct InstanceData {
        glm::vec4 uvBounds;  // minU, minV, maxU, maxV
        float face;
        float level;
        float _pad[2];
    };

    std::vector<InstanceData> m_instances;
    int m_instanceCount = 0;
};

}  // namespace astrocore
