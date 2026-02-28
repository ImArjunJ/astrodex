#include "physics/Octree.hpp"
#include "physics/CelestialBody.hpp"
#include <algorithm>
#include <limits>
#include <cmath>

namespace astrocore {

// OctreeNode implementation
int OctreeNode::getOctant(const glm::dvec3& pos) const {
    int octant = 0;
    if (pos.x >= center.x) octant |= 1;
    if (pos.y >= center.y) octant |= 2;
    if (pos.z >= center.z) octant |= 4;
    return octant;
}

glm::dvec3 OctreeNode::getChildCenter(int octant) const {
    double offset = halfSize * 0.5;
    return glm::dvec3(
        center.x + ((octant & 1) ? offset : -offset),
        center.y + ((octant & 2) ? offset : -offset),
        center.z + ((octant & 4) ? offset : -offset)
    );
}

// Octree implementation
Octree::Octree() = default;
Octree::~Octree() = default;

void Octree::clear() {
    m_root.reset();
    m_nodeCount = 0;
    m_maxDepth = 0;
}

void Octree::build(const std::vector<std::unique_ptr<CelestialBody>>& bodies) {
    clear();

    if (bodies.empty()) return;

    // Find bounding box of all bodies
    glm::dvec3 minBound(std::numeric_limits<double>::max());
    glm::dvec3 maxBound(std::numeric_limits<double>::lowest());

    for (const auto& body : bodies) {
        const glm::dvec3& pos = body->position();
        minBound = glm::min(minBound, pos);
        maxBound = glm::max(maxBound, pos);
    }

    // Create root node with some padding
    glm::dvec3 center = (minBound + maxBound) * 0.5;
    glm::dvec3 size = maxBound - minBound;
    double halfSize = std::max({size.x, size.y, size.z}) * 0.55;  // 10% padding

    // Ensure minimum size to avoid numerical issues
    halfSize = std::max(halfSize, 1.0e6);  // At least 1000 km

    m_root = std::make_unique<OctreeNode>(center, halfSize);
    m_nodeCount = 1;

    // Insert all bodies
    for (const auto& body : bodies) {
        insert(m_root.get(), body.get(), 0);
    }

    // Compute mass distribution for Barnes-Hut
    computeMassDistribution(m_root.get());
}

void Octree::insert(OctreeNode* node, CelestialBody* body, int depth) {
    m_maxDepth = std::max(m_maxDepth, depth);

    // Limit depth to prevent infinite recursion from coincident bodies
    constexpr int MAX_DEPTH = 50;
    if (depth >= MAX_DEPTH) {
        // Just add to this node's mass (bodies are essentially coincident)
        node->bodyCount++;
        return;
    }

    if (node->isEmpty()) {
        // Empty leaf - just store the body
        node->body = body;
        node->bodyCount = 1;
        return;
    }

    if (node->isExternal()) {
        // External node with one body - need to subdivide
        CelestialBody* existingBody = node->body;
        node->body = nullptr;

        // Create children
        for (int i = 0; i < 8; i++) {
            node->children[i] = std::make_unique<OctreeNode>(
                node->getChildCenter(i),
                node->halfSize * 0.5
            );
            m_nodeCount++;
        }

        // Re-insert the existing body
        int octant = node->getOctant(existingBody->position());
        insert(node->children[octant].get(), existingBody, depth + 1);
    }

    // Insert new body into appropriate child
    int octant = node->getOctant(body->position());
    insert(node->children[octant].get(), body, depth + 1);
    node->bodyCount++;
}

void Octree::computeMassDistribution(OctreeNode* node) {
    if (node == nullptr || node->isEmpty()) {
        return;
    }

    if (node->isExternal()) {
        // Leaf node with single body
        node->centerOfMass = node->body->position();
        node->totalMass = node->body->mass();
        return;
    }

    // Internal node - compute from children
    glm::dvec3 weightedPos(0.0);
    double totalMass = 0.0;

    for (int i = 0; i < 8; i++) {
        if (node->children[i] && !node->children[i]->isEmpty()) {
            computeMassDistribution(node->children[i].get());
            double childMass = node->children[i]->totalMass;
            weightedPos += node->children[i]->centerOfMass * childMass;
            totalMass += childMass;
        }
    }

    if (totalMass > 0.0) {
        node->centerOfMass = weightedPos / totalMass;
        node->totalMass = totalMass;
    }
}

glm::dvec3 Octree::calculateForce(const CelestialBody& body, double G, double theta) const {
    if (!m_root || m_root->isEmpty()) {
        return glm::dvec3(0.0);
    }
    return calculateForceRecursive(m_root.get(), body, G, theta);
}

glm::dvec3 Octree::calculateForceRecursive(const OctreeNode* node, const CelestialBody& body,
                                            double G, double theta) const {
    if (node == nullptr || node->isEmpty()) {
        return glm::dvec3(0.0);
    }

    // Skip self-interaction
    if (node->isExternal() && node->body->id() == body.id()) {
        return glm::dvec3(0.0);
    }

    glm::dvec3 direction = node->centerOfMass - body.position();
    double distSq = glm::dot(direction, direction);

    // Softening to prevent singularity when bodies are very close
    constexpr double SOFTENING = 1.0e6;  // 1000 km softening
    distSq += SOFTENING * SOFTENING;

    double dist = std::sqrt(distSq);

    // Barnes-Hut criterion: s/d < theta
    // where s = node size (2 * halfSize), d = distance
    double s = 2.0 * node->halfSize;

    if (node->isExternal() || (s / dist) < theta) {
        // Treat as single body - use center of mass approximation
        // F = G * m1 * m2 / r^2, direction toward center of mass
        double forceMag = G * body.mass() * node->totalMass / distSq;
        return glm::normalize(direction) * forceMag;
    }

    // Internal node that doesn't satisfy Barnes-Hut criterion
    // Recursively compute force from children
    glm::dvec3 totalForce(0.0);
    for (int i = 0; i < 8; i++) {
        if (node->children[i]) {
            totalForce += calculateForceRecursive(node->children[i].get(), body, G, theta);
        }
    }
    return totalForce;
}

double Octree::calculatePotentialEnergy(double G) const {
    // TODO: Implement potential energy calculation using the tree
    // For now, return 0 (would need full N^2 or tree-based calculation)
    return 0.0;
}

} // namespace astrocore
