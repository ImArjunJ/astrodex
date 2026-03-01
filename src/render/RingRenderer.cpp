#include "render/RingRenderer.hpp"
#include "core/Logger.hpp"
#include <random>
#include <cmath>

namespace astrocore {

RingRenderer::~RingRenderer() {
    shutdown();
}

void RingRenderer::init() {
    // Simple point/billboard shader for ring particles
    // Per-particle colors passed via instance data for multi-band support
    const char* vertSrc = R"(
        #version 410 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aInstancePos;
        layout(location = 2) in float aSize;
        layout(location = 3) in vec3 aColor;
        layout(location = 4) in float aOpacity;

        uniform mat4 uViewProj;
        uniform vec3 uPlanetPos;
        uniform float uPlanetRadius;

        out vec3 vColor;
        out float vOpacity;

        void main() {
            vec3 worldPos = uPlanetPos + aInstancePos * uPlanetRadius;
            gl_Position = uViewProj * vec4(worldPos, 1.0);
            gl_PointSize = aSize * uPlanetRadius * 50.0 / gl_Position.w;
            gl_PointSize = clamp(gl_PointSize, 1.0, 8.0);
            vColor = aColor;
            vOpacity = aOpacity;
        }
    )";

    const char* fragSrc = R"(
        #version 410 core
        in vec3 vColor;
        in float vOpacity;

        uniform vec3 uSunDir;

        out vec4 FragColor;

        void main() {
            // Circular point
            vec2 coord = gl_PointCoord * 2.0 - 1.0;
            float dist = length(coord);
            if (dist > 1.0) discard;

            // Soft edges
            float alpha = smoothstep(1.0, 0.5, dist) * vOpacity;

            // Simple lighting
            float light = 0.5 + 0.5 * max(dot(normalize(vec3(coord, 0.5)), uSunDir), 0.0);
            vec3 color = vColor * light;

            FragColor = vec4(color, alpha);
        }
    )";

    m_shader.loadFromSource(vertSrc, fragSrc);

    // Create VAO for point rendering
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_instanceVBO);

    glBindVertexArray(m_vao);

    // Dummy vertex (we use instancing)
    float point[] = {0.0f, 0.0f, 0.0f};
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(point), point, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindVertexArray(0);

    LOG_INFO("RingRenderer initialized");
}

void RingRenderer::shutdown() {
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_instanceVBO) glDeleteBuffers(1, &m_instanceVBO);
    m_vao = m_vbo = m_instanceVBO = 0;
    m_particles.clear();
    m_particleCount = 0;
}

void RingRenderer::generateRing(const RingParams& params, float planetRadius, float planetMass) {
    if (!params.enabled || params.bands.empty() || params.totalParticles <= 0) {
        m_particles.clear();
        m_particleCount = 0;
        return;
    }

    m_planetRadius = planetRadius;

    // Calculate total density weight across all bands
    float totalDensityWeight = 0.0f;
    for (const auto& band : params.bands) {
        float bandArea = (band.outerRadius * band.outerRadius - band.innerRadius * band.innerRadius);
        totalDensityWeight += bandArea * band.density;
    }

    if (totalDensityWeight <= 0.0f) {
        m_particles.clear();
        m_particleCount = 0;
        return;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159265f);
    std::uniform_real_distribution<float> heightDist(-params.thickness, params.thickness);
    std::uniform_real_distribution<float> sizeDist(0.001f, 0.005f);

    m_particles.clear();
    m_particles.reserve(params.totalParticles);

    // Generate particles for each band proportional to area * density
    for (size_t bandIdx = 0; bandIdx < params.bands.size(); bandIdx++) {
        const auto& band = params.bands[bandIdx];
        float bandArea = (band.outerRadius * band.outerRadius - band.innerRadius * band.innerRadius);
        float bandWeight = bandArea * band.density;
        int bandParticles = static_cast<int>((bandWeight / totalDensityWeight) * params.totalParticles);

        std::uniform_real_distribution<float> radiusDist(band.innerRadius, band.outerRadius);

        for (int i = 0; i < bandParticles; i++) {
            float angle = angleDist(gen);
            float radius = radiusDist(gen);
            float height = heightDist(gen);

            // Keplerian angular velocity: omega = sqrt(GM/r^3) -> simplified to 1/r^1.5
            float omega = 1.0f / std::pow(radius, 1.5f);

            RingParticle particle;
            particle.position = glm::vec3(
                radius * std::cos(angle),
                height,
                radius * std::sin(angle)
            );
            particle.orbitRadius = radius;
            particle.angle = angle;
            particle.speed = omega;
            particle.size = sizeDist(gen);
            particle.bandIndex = bandIdx;

            m_particles.push_back(particle);
        }
    }

    m_particleCount = static_cast<int>(m_particles.size());

    // Find overall ring extent for logging
    float minRadius = params.bands[0].innerRadius;
    float maxRadius = params.bands[0].outerRadius;
    for (const auto& band : params.bands) {
        minRadius = std::min(minRadius, band.innerRadius);
        maxRadius = std::max(maxRadius, band.outerRadius);
    }

    LOG_INFO("Generated {} ring particles across {} bands (r: {:.2f} - {:.2f})",
             m_particleCount, params.bands.size(), minRadius, maxRadius);
}

void RingRenderer::update(float deltaTime, float planetMass) {
    if (m_particles.empty()) return;

    // Update particle positions based on orbital motion
    float timeScale = 0.5f;  // Slow down for visibility
    for (auto& p : m_particles) {
        p.angle += p.speed * deltaTime * timeScale;
        if (p.angle > 2.0f * 3.14159265f) p.angle -= 2.0f * 3.14159265f;

        p.position.x = p.orbitRadius * std::cos(p.angle);
        p.position.z = p.orbitRadius * std::sin(p.angle);
    }
}

void RingRenderer::render(const glm::mat4& viewProj, const glm::vec3& planetPos,
                          const glm::vec3& cameraPos, const RingParams& params) {
    if (m_particles.empty() || !params.enabled || params.bands.empty()) return;

    // Pack instance data: position (3), size (1), color (3), opacity (1) = 8 floats
    std::vector<float> instanceData;
    instanceData.reserve(m_particles.size() * 8);

    for (const auto& p : m_particles) {
        // Get the band this particle belongs to
        const auto& band = params.bands[std::min(p.bandIndex, params.bands.size() - 1)];

        // Calculate distance ratio within this band (0-1)
        float range = band.outerRadius - band.innerRadius;
        float distRatio = (range > 0.0f) ? (p.orbitRadius - band.innerRadius) / range : 0.5f;
        distRatio = glm::clamp(distRatio, 0.0f, 1.0f);

        // Interpolate color based on position in band
        glm::vec3 color = glm::mix(band.innerColor, band.outerColor, distRatio);

        instanceData.push_back(p.position.x);
        instanceData.push_back(p.position.y);
        instanceData.push_back(p.position.z);
        instanceData.push_back(p.size);
        instanceData.push_back(color.r);
        instanceData.push_back(color.g);
        instanceData.push_back(color.b);
        instanceData.push_back(band.opacity);
    }

    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(float),
                 instanceData.data(), GL_DYNAMIC_DRAW);

    // Set up instance attributes
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);

    // Instance position (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glVertexAttribDivisor(1, 1);

    // Instance size (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glVertexAttribDivisor(2, 1);

    // Instance color (location 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(3, 1);

    // Instance opacity (location 4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(7 * sizeof(float)));
    glVertexAttribDivisor(4, 1);

    // Render
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glDepthMask(GL_FALSE);  // Don't write to depth buffer

    m_shader.use();
    m_shader.setMat4("uViewProj", viewProj);
    m_shader.setVec3("uPlanetPos", planetPos);
    m_shader.setFloat("uPlanetRadius", m_planetRadius);
    m_shader.setVec3("uSunDir", glm::normalize(glm::vec3(1.0f, 0.5f, 0.5f)));

    glDrawArraysInstanced(GL_POINTS, 0, 1, m_particleCount);

    m_shader.unuse();

    glDepthMask(GL_TRUE);
    glDisable(GL_PROGRAM_POINT_SIZE);
    glDisable(GL_BLEND);

    glVertexAttribDivisor(1, 0);
    glVertexAttribDivisor(2, 0);
    glVertexAttribDivisor(3, 0);
    glVertexAttribDivisor(4, 0);
    glBindVertexArray(0);
}

}  // namespace astrocore
