#version 410 core

in vec3 v_worldPos;
in vec3 v_localPos;

out vec4 frag_color;

uniform vec3 u_cameraPos;
uniform vec3 u_sunDirection;
uniform vec3 u_sunColor;
uniform float u_sunIntensity;
uniform float u_planetRadius;
uniform float u_atmosphereRadius;
uniform vec3 u_rayleighCoeff;
uniform float u_atmosphereDensity;

const float PI = 3.14159265359;
const int SCATTER_STEPS = 16;
const int OPTICAL_DEPTH_STEPS = 8;

float rayleighPhase(float cosAngle) {
    return 0.75 * (1.0 + cosAngle * cosAngle);
}

float miePhase(float cosAngle, float g) {
    float g2 = g * g;
    float num = (1.0 - g2);
    float denom = pow(1.0 + g2 - 2.0 * g * cosAngle, 1.5);
    return num / (4.0 * PI * denom);
}

// Ray-sphere intersection
vec2 raySphere(vec3 ro, vec3 rd, float radius) {
    float b = dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float d = b * b - c;
    if (d < 0.0) return vec2(-1.0);
    d = sqrt(d);
    return vec2(-b - d, -b + d);
}

float atmosphereDensity(vec3 point) {
    float height = length(point) - u_planetRadius;
    float scaleHeight = (u_atmosphereRadius - u_planetRadius) * 0.25;
    return exp(-height / scaleHeight) * u_atmosphereDensity;
}

float opticalDepth(vec3 rayOrigin, vec3 rayDir, float rayLength) {
    float stepSize = rayLength / float(OPTICAL_DEPTH_STEPS);
    vec3 point = rayOrigin;
    float depth = 0.0;

    for (int i = 0; i < OPTICAL_DEPTH_STEPS; i++) {
        depth += atmosphereDensity(point) * stepSize;
        point += rayDir * stepSize;
    }

    return depth;
}

vec3 calculateScattering(vec3 rayOrigin, vec3 rayDir, float rayLength) {
    vec3 scatterPoint = rayOrigin;
    float stepSize = rayLength / float(SCATTER_STEPS);

    vec3 totalScattering = vec3(0.0);
    float viewOpticalDepth = 0.0;

    float cosAngle = dot(rayDir, u_sunDirection);
    float phaseR = rayleighPhase(cosAngle);
    float phaseM = miePhase(cosAngle, 0.76);

    for (int i = 0; i < SCATTER_STEPS; i++) {
        float height = length(scatterPoint) - u_planetRadius;
        float density = atmosphereDensity(scatterPoint);

        viewOpticalDepth += density * stepSize;

        // Light ray to sun
        vec2 sunHit = raySphere(scatterPoint, u_sunDirection, u_atmosphereRadius);
        float sunRayLength = sunHit.y;

        // Check if blocked by planet
        vec2 planetHit = raySphere(scatterPoint, u_sunDirection, u_planetRadius);
        if (planetHit.x > 0.0) {
            scatterPoint += rayDir * stepSize;
            continue;
        }

        float sunOpticalDepth = opticalDepth(scatterPoint, u_sunDirection, sunRayLength);

        // Transmittance
        vec3 tau = u_rayleighCoeff * 4e5 * (viewOpticalDepth + sunOpticalDepth);
        vec3 transmittance = exp(-tau);

        // Scattering contribution
        totalScattering += density * transmittance * stepSize;

        scatterPoint += rayDir * stepSize;
    }

    vec3 rayleigh = totalScattering * u_rayleighCoeff * 4e5 * phaseR;
    vec3 mie = totalScattering * vec3(2e-5) * phaseM;

    return (rayleigh + mie) * u_sunIntensity * u_sunColor;
}

void main() {
    vec3 rayDir = normalize(v_worldPos - u_cameraPos);
    vec3 rayOrigin = u_cameraPos;

    // Intersect with atmosphere
    vec2 atmosHit = raySphere(rayOrigin, rayDir, u_atmosphereRadius);
    vec2 planetHit = raySphere(rayOrigin, rayDir, u_planetRadius);

    if (atmosHit.x < 0.0 && atmosHit.y < 0.0) {
        discard;
    }

    float tStart = max(atmosHit.x, 0.0);
    float tEnd = atmosHit.y;

    // If we hit the planet, stop at the surface
    if (planetHit.x > 0.0) {
        tEnd = min(tEnd, planetHit.x);
    }

    float rayLength = tEnd - tStart;
    if (rayLength <= 0.0) {
        discard;
    }

    vec3 startPoint = rayOrigin + rayDir * tStart;
    vec3 color = calculateScattering(startPoint, rayDir, rayLength);

    // Limb brightening
    float limb = 1.0 - abs(dot(normalize(v_localPos), rayDir));
    limb = pow(limb, 2.0);

    // Tone mapping
    color = 1.0 - exp(-color * 1.5);

    // Alpha based on scattering intensity
    float alpha = clamp(length(color) * 2.0 + limb * 0.3, 0.0, 0.9);

    frag_color = vec4(color, alpha);
}
