#pragma once

#include <glm/glm.hpp>
#include <array>
#include <memory>

namespace astrocore {

class Camera;
class TerrainMesh;

// Lightweight quadtree node - NO GL objects, just bounds and hierarchy
// Mesh rendering is done via shared TerrainMesh instancing
class QuadtreeNode {
public:
    static constexpr int MAX_LEVEL = 5;  // Much lower - don't over-split
    static constexpr float SPLIT_THRESHOLD = 4.0f;  // Higher = less splitting
    static constexpr float MERGE_THRESHOLD = 2.0f;

    QuadtreeNode(int face, int level, const glm::vec2& min, const glm::vec2& max,
                 QuadtreeNode* parent = nullptr);
    ~QuadtreeNode() = default;

    // Prevent copying
    QuadtreeNode(const QuadtreeNode&) = delete;
    QuadtreeNode& operator=(const QuadtreeNode&) = delete;

    // LOD update - returns number of visible leaf nodes
    int update(const Camera& camera, const glm::vec3& planetCenter, float planetRadius);

    // Collect visible leaf nodes for instanced rendering
    void collectVisibleNodes(TerrainMesh& mesh, const Camera& camera,
                             const glm::vec3& planetCenter, float planetRadius);

    // Accessors
    int getFace() const { return m_face; }
    int getLevel() const { return m_level; }
    bool isSplit() const { return m_children[0] != nullptr; }
    bool isLeaf() const { return !isSplit(); }
    glm::vec2 getMinUV() const { return m_minUV; }
    glm::vec2 getMaxUV() const { return m_maxUV; }

    // Cube-to-sphere projection
    static glm::vec3 cubeToSphere(int face, const glm::vec2& uv);

    glm::vec3 getCenter(const glm::vec3& planetCenter, float planetRadius) const;
    float getGeometricError(float planetRadius) const;

private:
    void split();
    void merge();

    float calculateScreenError(const Camera& camera, const glm::vec3& planetCenter,
                               float planetRadius) const;
    bool isInFrustum(const Camera& camera, const glm::vec3& planetCenter,
                     float planetRadius) const;

    int m_face;
    int m_level;
    glm::vec2 m_minUV;
    glm::vec2 m_maxUV;

    QuadtreeNode* m_parent;
    std::array<std::unique_ptr<QuadtreeNode>, 4> m_children;

    mutable bool m_visible = true;
};

}  // namespace astrocore
