#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace astrocore {

struct StarVertex {
    glm::vec3 position;   // xyz in parsecs
    float     magnitude;  // apparent magnitude
    glm::vec3 color;      // RGB from B-V color index
};

struct StarInfo {
    std::string name;
    std::string spectralType;
    std::string constellation;
    float       magnitude;
    float       distance;
    glm::vec3   position;
};

class StarData {
public:
    bool load(const std::string& csvPath);

    const std::vector<StarVertex>& vertices() const { return m_vertices; }
    size_t count() const { return m_vertices.size(); }

    // Brute-force nearest star lookup
    StarInfo findNearest(const glm::vec3& pos) const;

private:
    static glm::vec3 bvToRGB(float bv);

    std::vector<StarVertex> m_vertices;

    // Full info for named lookups
    struct StarRecord {
        std::string name;
        std::string spectralType;
        std::string constellation;
        float       magnitude;
        float       distance;
        glm::vec3   position;
    };
    std::vector<StarRecord> m_records;
};

} // namespace astrocore
