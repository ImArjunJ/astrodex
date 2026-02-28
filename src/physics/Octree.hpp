#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <array>

namespace astrocore {

// Forward declaration
class CelestialBody;

// Barnes-Hut Octree for O(N log N) gravitational force calculation
// Instead of computing N^2 pairwise forces, we approximate distant groups
// of bodies as single point masses located at their center of mass.

struct OctreeNode {
    // Bounding box
    glm::dvec3 center;
    double halfSize;

    // Mass properties (for Barnes-Hut approximation)
    glm::dvec3 centerOfMass{0.0};
    double totalMass = 0.0;

    // Children (8 octants) - null if leaf
    std::array<std::unique_ptr<OctreeNode>, 8> children;

    // Body pointer (only for leaf nodes with single body)
    CelestialBody* body = nullptr;
    int bodyCount = 0;

    OctreeNode(const glm::dvec3& c, double hs) : center(c), halfSize(hs) {}

    // Move-only (contains unique_ptrs)
    OctreeNode(OctreeNode&&) noexcept = default;
    OctreeNode& operator=(OctreeNode&&) noexcept = default;
    OctreeNode(const OctreeNode&) = delete;
    OctreeNode& operator=(const OctreeNode&) = delete;

    bool isLeaf() const { return children[0] == nullptr; }
    bool isEmpty() const { return bodyCount == 0; }
    bool isExternal() const { return isLeaf() && bodyCount == 1; }

    // Get octant index for a position (0-7)
    int getOctant(const glm::dvec3& pos) const;

    // Get center of child octant
    glm::dvec3 getChildCenter(int octant) const;
};

class Octree {
public:
    // Barnes-Hut theta parameter: ratio of node size to distance
    // theta = 0: exact N^2 calculation
    // theta = 0.5: good accuracy/speed balance
    // theta = 1.0: faster but less accurate
    static constexpr double DEFAULT_THETA = 0.5;

    Octree();
    ~Octree();

    // Move-only
    Octree(Octree&&) noexcept = default;
    Octree& operator=(Octree&&) noexcept = default;
    Octree(const Octree&) = delete;
    Octree& operator=(const Octree&) = delete;

    // Build tree from bodies
    void build(const std::vector<std::unique_ptr<CelestialBody>>& bodies);
    void clear();

    // Calculate gravitational force on a body using Barnes-Hut approximation
    glm::dvec3 calculateForce(const CelestialBody& body, double G, double theta = DEFAULT_THETA) const;

    // Calculate potential energy of the system
    double calculatePotentialEnergy(double G) const;

    // Statistics
    int getNodeCount() const { return m_nodeCount; }
    int getMaxDepth() const { return m_maxDepth; }

private:
    void insert(OctreeNode* node, CelestialBody* body, int depth);
    void computeMassDistribution(OctreeNode* node);
    glm::dvec3 calculateForceRecursive(const OctreeNode* node, const CelestialBody& body,
                                        double G, double theta) const;

    std::unique_ptr<OctreeNode> m_root;
    int m_nodeCount = 0;
    int m_maxDepth = 0;
};

} // namespace astrocore
