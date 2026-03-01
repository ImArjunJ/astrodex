#version 410 core

in vec3 vColor;
in float vBrightness;
in vec2 vStreakDir;
in float vWarp;
in float vRadialDist;

uniform float uDebugMode;

out vec4 fragColor;

void main() {
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);

    // Debug mode: flat white dots
    if (uDebugMode > 0.5) {
        if (r2 > 1.0) discard;
        float a = 1.0 - r2;
        fragColor = vec4(vec3(a), a);
        return;
    }

    // ── Warp streak mode ────────────────────────────────────────────────
    if (vWarp > 0.01) {
        // How much this star participates in streaking (based on brightness)
        float brightFrac = clamp((vBrightness - 0.25) / 1.0, 0.0, 1.0);
        float effectiveWarp = vWarp * brightFrac;

        // Dim stars: fall back to normal circular rendering (dimmed)
        if (effectiveWarp < 0.02) {
            if (r2 > 1.0) discard;
            float core = exp(-r2 * 8.0);
            float halo = exp(-r2 * 2.0);
            float a = core * 0.8 + halo * 0.3;
            if (a < 0.01) discard;
            vec3 c = mix(vColor * halo, mix(vColor, vec3(1.0), 0.6) * core, core / max(core + halo, 0.001));
            fragColor = vec4(c * min(vBrightness, 50.0), a);
            return;
        }

        // Rotate coord so streak aligns with radial direction from screen center
        vec2 dir = normalize(vStreakDir);
        mat2 rot = mat2(dir.x, dir.y, -dir.y, dir.x);
        vec2 aligned = rot * coord;

        // Streak shape: length proportional to brightness and warp
        float streakLen = mix(1.0, 2.0 + vRadialDist * 5.0, effectiveWarp);
        float sx = aligned.x / streakLen;
        float sy = aligned.y * (1.0 + effectiveWarp * 2.0);

        float d2 = sx * sx + sy * sy;
        if (d2 > 1.0) discard;

        float core = exp(-d2 * 6.0);
        float trail = exp(-sx * sx * 2.0) * exp(-sy * sy * 12.0);
        float alpha = core * 0.8 + trail * 0.3;
        if (alpha < 0.005) discard;

        // Shift bright streaks toward white/blue
        vec3 warpColor = mix(vColor, vec3(0.8, 0.9, 1.0), effectiveWarp * 0.4);
        vec3 color = mix(warpColor, vec3(1.0), core * 0.6);
        color *= min(vBrightness, 50.0);

        fragColor = vec4(color, alpha);
        return;
    }

    // ── Normal star rendering ───────────────────────────────────────────
    if (r2 > 1.0) discard;

    float core = exp(-r2 * 8.0);
    float halo = exp(-r2 * 2.0);
    float alpha = core * 0.8 + halo * 0.3;
    if (alpha < 0.01) discard;

    vec3 coreColor = mix(vColor, vec3(1.0), 0.6);
    vec3 color = mix(vColor * halo, coreColor * core, core / max(core + halo, 0.001));

    float brightness = min(vBrightness, 50.0);
    color *= brightness;

    fragColor = vec4(color, alpha);
}
