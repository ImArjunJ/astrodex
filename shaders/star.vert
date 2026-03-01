#version 410 core

layout(location = 0) in vec3 aPosition;   // xyz in parsecs
layout(location = 1) in float aMagnitude;  // apparent magnitude (from Earth)
layout(location = 2) in vec3 aColor;       // RGB from B-V

uniform mat4 uView;
uniform mat4 uProjection;
uniform float uPointScale;
uniform float uBrightnessBoost;
uniform float uWarpFactor;       // 0 = normal, 1 = full warp

out vec3 vColor;
out float vBrightness;
out vec2 vStreakDir;    // screen-space radial direction for streak effect
out float vWarp;        // passed to fragment shader
out float vRadialDist;  // distance from screen center (0-1+)

void main() {
    vec4 viewPos = uView * vec4(aPosition, 1.0);
    gl_Position = uProjection * viewPos;

    float cameraDist = length(viewPos.xyz);

    // Catalog flux is apparent magnitude as seen from Sol (origin).
    // Recompute flux at the camera's actual distance using inverse-square law.
    float catalogFlux = pow(10.0, -0.4 * aMagnitude);
    float solDist = length(aPosition);
    float flux;
    if (solDist < 1.0) {
        flux = catalogFlux / max(cameraDist * cameraDist, 0.001);
        flux = max(flux, catalogFlux);
    } else {
        float ratio = solDist / max(cameraDist, 0.001);
        flux = catalogFlux * ratio * ratio;
    }

    // Point size: brighter and closer -> larger
    float size = uPointScale * pow(max(flux, 1e-10), 0.25) * 30.0;

    // Perceptual brightness (compute BEFORE warp so we can scale by it)
    float logBright = log2(flux * 1e6 + 1.0) / 20.0;
    float baseBrightness = max(0.08, logBright);

    // During warp: scale streak size by star brightness
    // Only bright stars (baseBrightness > ~0.3) get long streaks
    vec2 ndc = gl_Position.xy / gl_Position.w;
    float radial = length(ndc);
    vRadialDist = radial;

    float brightFrac = clamp((baseBrightness - 0.2) / 0.8, 0.0, 1.0); // 0 for dim, 1 for bright
    float warpSizeBoost = 1.0 + uWarpFactor * brightFrac * (1.5 + radial * 6.0);
    size *= warpSizeBoost;

    gl_PointSize = clamp(size, 1.0, 128.0);

    // Brightness: dim stars get dimmer during warp, bright stars get mild boost
    vBrightness = baseBrightness * uBrightnessBoost;
    float warpDim = mix(0.4, 1.0 + brightFrac * 0.8, brightFrac); // dim stars → 40%, bright → 180%
    vBrightness *= mix(1.0, warpDim, uWarpFactor);

    vColor = aColor;
    vWarp = uWarpFactor;

    // Compute radial streak direction in point-sprite space
    // Direction from screen center to this star (in NDC)
    float radLen = max(length(ndc), 0.001);
    vStreakDir = ndc / radLen;
}
