#include "render/SphereRenderer.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

namespace astrocore {

SphereRenderer::SphereRenderer() = default;

SphereRenderer::~SphereRenderer() {
    shutdown();
}

void SphereRenderer::init() {
    if (m_initialized) return;

    createSphereMesh(32);  // 32 segments for decent quality
    createShader();

    m_instances.reserve(MAX_INSTANCES);
    m_initialized = true;
}

void SphereRenderer::shutdown() {
    if (!m_initialized) return;

    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);
    if (m_instanceVBO) glDeleteBuffers(1, &m_instanceVBO);

    m_vao = m_vbo = m_ebo = m_instanceVBO = 0;
    m_initialized = false;
}

void SphereRenderer::createSphereMesh(int segments) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    // Generate sphere vertices
    for (int lat = 0; lat <= segments; lat++) {
        float theta = static_cast<float>(lat) * glm::pi<float>() / static_cast<float>(segments);
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (int lon = 0; lon <= segments; lon++) {
            float phi = static_cast<float>(lon) * 2.0f * glm::pi<float>() / static_cast<float>(segments);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            // Position (normalized, will be scaled by radius)
            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            // Normal is same as position for unit sphere
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(x);  // normal x
            vertices.push_back(y);  // normal y
            vertices.push_back(z);  // normal z
        }
    }

    // Generate indices
    for (int lat = 0; lat < segments; lat++) {
        for (int lon = 0; lon < segments; lon++) {
            int first = lat * (segments + 1) + lon;
            int second = first + segments + 1;

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
    glBindVertexArray(m_vao);

    // Vertex buffer
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);

    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));

    // Index buffer
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Instance buffer
    glGenBuffers(1, &m_instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCES * sizeof(InstanceData), nullptr, GL_DYNAMIC_DRAW);

    // Instance attributes
    // Position + radius (vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), nullptr);
    glVertexAttribDivisor(2, 1);

    // Color + emissive (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), reinterpret_cast<void*>(4 * sizeof(float)));
    glVertexAttribDivisor(3, 1);

    // Atmosphere color + hasAtmosphere (vec4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), reinterpret_cast<void*>(8 * sizeof(float)));
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);
}

void SphereRenderer::createShader() {
    if (!m_shader.loadFromFiles("shaders/sphere.vert", "shaders/sphere.frag")) {
        // Fallback to embedded shader if files not found
        const char* vertexSource = R"(
#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aInstancePosRadius;
layout(location = 3) in vec4 aInstanceColorEmissive;
layout(location = 4) in vec4 aInstanceAtmosphere;
uniform mat4 uViewProjection;
uniform vec3 uCameraPos;
out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out float vEmissive;
out vec3 vAtmosphereColor;
out float vHasAtmosphere;
out vec3 vViewDir;
void main() {
    vec3 worldPos = aPos * aInstancePosRadius.w + aInstancePosRadius.xyz;
    vWorldPos = worldPos;
    vNormal = aNormal;
    vColor = aInstanceColorEmissive.xyz;
    vEmissive = aInstanceColorEmissive.w;
    vAtmosphereColor = aInstanceAtmosphere.xyz;
    vHasAtmosphere = aInstanceAtmosphere.w;
    vViewDir = normalize(uCameraPos - worldPos);
    gl_Position = uViewProjection * vec4(worldPos, 1.0);
}
)";
        const char* fragmentSource = R"(
#version 410 core
in vec3 vNormal;
in vec3 vColor;
in float vEmissive;
in vec3 vAtmosphereColor;
in float vHasAtmosphere;
in vec3 vViewDir;
uniform vec3 uSunDir;
uniform float uSunIntensity;
out vec4 FragColor;
void main() {
    vec3 normal = normalize(vNormal);
    float NdotL = max(dot(normal, uSunDir), 0.0);
    float fresnel = pow(1.0 - max(dot(normal, vViewDir), 0.0), 3.0);
    vec3 color;
    if (vEmissive > 0.5) {
        color = vColor + vColor * fresnel * 2.0;
    } else {
        float lighting = 0.08 + NdotL * uSunIntensity;
        color = vColor * lighting;
        if (vHasAtmosphere > 0.5) color += vAtmosphereColor * fresnel * 0.5;
        color += vec3(0.1, 0.15, 0.2) * fresnel * 0.3;
    }
    FragColor = vec4(color, 1.0);
}
)";
        m_shader.loadFromSource(vertexSource, fragmentSource);
    }
}

void SphereRenderer::begin(const glm::mat4& viewProjection, const glm::vec3& cameraPos,
                           const glm::vec3& sunDir, float sunIntensity) {
    m_viewProjection = viewProjection;
    m_cameraPos = cameraPos;
    m_sunDir = sunDir;
    m_sunIntensity = sunIntensity;
    m_instances.clear();
}

void SphereRenderer::drawSphere(const glm::vec3& position, float radius,
                                 const glm::vec3& color, float emissive,
                                 bool hasAtmosphere, const glm::vec3& atmosphereColor) {
    if (m_instances.size() >= MAX_INSTANCES) return;

    InstanceData instance;
    instance.position = position;
    instance.radius = radius;
    instance.color = color;
    instance.emissive = emissive;
    instance.atmosphereColor = atmosphereColor;
    instance.hasAtmosphere = hasAtmosphere ? 1.0f : 0.0f;
    m_instances.push_back(instance);
}

void SphereRenderer::end() {
    if (m_instances.empty()) return;

    m_renderedCount = static_cast<int>(m_instances.size());

    // Upload instance data
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_instances.size() * sizeof(InstanceData), m_instances.data());

    // Render
    m_shader.use();
    m_shader.setMat4("uViewProjection", m_viewProjection);
    m_shader.setVec3("uCameraPos", m_cameraPos);
    m_shader.setVec3("uSunDir", m_sunDir);
    m_shader.setFloat("uSunIntensity", m_sunIntensity);

    glBindVertexArray(m_vao);
    glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr, m_renderedCount);
    glBindVertexArray(0);

    m_shader.unuse();
}

} // namespace astrocore
