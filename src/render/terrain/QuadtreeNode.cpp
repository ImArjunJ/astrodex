#include "render/terrain/QuadtreeNode.hpp"
#include "render/terrain/TerrainMesh.hpp"
#include "render/Camera.hpp"
#include <cmath>

namespace astrocore {

QuadtreeNode::QuadtreeNode(int face, int level, const glm::vec2& min, const glm::vec2& max,
                           QuadtreeNode* parent)
    : m_face(face)
    , m_level(level)
    , m_minUV(min)
    , m_maxUV(max)
    , m_parent(parent) {
    // No GL resources - just data
}

glm::vec3 QuadtreeNode::cubeToSphere(int face, const glm::vec2& uv) {
    glm::vec3 pos;
    switch (face) {
        case 0: pos = glm::vec3( 1.0f, uv.y, -uv.x); break;  // +X
        case 1: pos = glm::vec3(-1.0f, uv.y,  uv.x); break;  // -X
        case 2: pos = glm::vec3(uv.x,  1.0f, -uv.y); break;  // +Y
        case 3: pos = glm::vec3(uv.x, -1.0f,  uv.y); break;  // -Y
        case 4: pos = glm::vec3(uv.x, uv.y,  1.0f); break;   // +Z
        default: pos = glm::vec3(-uv.x, uv.y, -1.0f); break; // -Z
    }
    return glm::normalize(pos);
}

glm::vec3 QuadtreeNode::getCenter(const glm::vec3& planetCenter, float planetRadius) const {
    glm::vec2 centerUV = (m_minUV + m_maxUV) * 0.5f;
    glm::vec3 dir = cubeToSphere(m_face, centerUV);
    return planetCenter + dir * planetRadius;
}

float QuadtreeNode::getGeometricError(float planetRadius) const {
    // Error halves with each level
    return (planetRadius * 0.5f) / static_cast<float>(1 << m_level);
}

float QuadtreeNode::calculateScreenError(const Camera& camera, const glm::vec3& planetCenter,
                                          float planetRadius) const {
    glm::vec3 center = getCenter(planetCenter, planetRadius);
    float distance = glm::length(camera.getPosition() - center);
    if (distance < 0.001f) distance = 0.001f;

    float geometricError = getGeometricError(planetRadius);

    // Simple ratio: how big does this patch appear on screen?
    // Split when patch is large relative to distance
    return (geometricError / distance) * 100.0f;
}

bool QuadtreeNode::isInFrustum(const Camera& camera, const glm::vec3& planetCenter,
                               float planetRadius) const {
    // For now, disable frustum culling - render all visible nodes
    // Proper frustum culling needs to account for large patch angular size
    // TODO: Implement proper sphere-patch frustum intersection

    // Only cull nodes that are completely behind the camera
    glm::vec3 center = getCenter(planetCenter, planetRadius);
    glm::vec3 toCenter = center - camera.getPosition();
    glm::vec3 forward = camera.getForward();

    float patchRadius = getGeometricError(planetRadius) * 2.0f;
    float distAlongForward = glm::dot(toCenter, forward);

    // Behind camera check with generous margin
    if (distAlongForward < -patchRadius * 2.0f) {
        return false;
    }

    return true;
}

int QuadtreeNode::update(const Camera& camera, const glm::vec3& planetCenter, float planetRadius) {
    m_visible = isInFrustum(camera, planetCenter, planetRadius);

    if (!m_visible) {
        return 0;
    }

    float screenError = calculateScreenError(camera, planetCenter, planetRadius);

    if (isSplit()) {
        if (screenError < MERGE_THRESHOLD) {
            bool canMerge = true;
            for (const auto& child : m_children) {
                if (child && !child->isLeaf()) {
                    canMerge = false;
                    break;
                }
            }
            if (canMerge) {
                merge();
                return 1;
            }
        }

        int count = 0;
        for (auto& child : m_children) {
            if (child) {
                count += child->update(camera, planetCenter, planetRadius);
            }
        }
        return count;
    } else {
        if (screenError > SPLIT_THRESHOLD && m_level < MAX_LEVEL) {
            split();
            int count = 0;
            for (auto& child : m_children) {
                if (child) {
                    count += child->update(camera, planetCenter, planetRadius);
                }
            }
            return count;
        }
        return 1;
    }
}

void QuadtreeNode::collectVisibleNodes(TerrainMesh& mesh, const Camera& camera,
                                        const glm::vec3& planetCenter, float planetRadius) {
    if (!m_visible) return;

    if (isSplit()) {
        for (auto& child : m_children) {
            if (child) {
                child->collectVisibleNodes(mesh, camera, planetCenter, planetRadius);
            }
        }
    } else {
        mesh.addInstance(m_face, m_minUV, m_maxUV, m_level);
    }
}

void QuadtreeNode::split() {
    if (isSplit()) return;

    glm::vec2 center = (m_minUV + m_maxUV) * 0.5f;

    m_children[0] = std::make_unique<QuadtreeNode>(m_face, m_level + 1, m_minUV, center, this);
    m_children[1] = std::make_unique<QuadtreeNode>(m_face, m_level + 1,
        glm::vec2(center.x, m_minUV.y), glm::vec2(m_maxUV.x, center.y), this);
    m_children[2] = std::make_unique<QuadtreeNode>(m_face, m_level + 1, center, m_maxUV, this);
    m_children[3] = std::make_unique<QuadtreeNode>(m_face, m_level + 1,
        glm::vec2(m_minUV.x, center.y), glm::vec2(center.x, m_maxUV.y), this);
}

void QuadtreeNode::merge() {
    for (auto& child : m_children) {
        child.reset();
    }
}

}  // namespace astrocore
