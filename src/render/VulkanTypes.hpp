#pragma once

#include <cstdint>

namespace astrocore {

// Must match PlanetUniforms in planet_vk.frag exactly (std140 layout).
// mat3 stored as 3 x vec4 (48 bytes) per std140 rules.
// Total: 64 + 48 + 27*16 = 544 bytes.
struct alignas(16) PlanetUniformsVk {
    // mat4 invView (64 bytes)
    float invView[16];

    // mat3 planetRotation stored as 3 x vec4 (48 bytes, std140)
    float planetRot_col0[4]; // col0.xyz, pad
    float planetRot_col1[4]; // col1.xyz, pad
    float planetRot_col2[4]; // col2.xyz, pad

    // vec4 groups (16 bytes each)
    float camPosX, camPosY, camPosZ, time;              // cameraPos_time
    float planetX, planetY, planetZ, radius;             // planetPos_radius
    float resX, resY, rotOffset, rotSpeed;               // resolution_rot
    float noiseStr, quality, terrainScale, domainWarp;   // noiseParams
    float fbmPersist, fbmLac, fbmExp, fbmOct;           // fbmParams
    float ridged, crater, continent, waterLevel;          // terrainFeatures
    float bandStr, bandFreq, polarCap, _pad0;            // bandingPolar
    float cloudDensity, cloudScale, cloudSpeed, cloudAlt; // cloudParams1
    float cloudThick, sunInt, ambLight, atmoDensity;     // cloudParams2
    float atmoR, atmoG, atmoB, _pad1;                   // atmosphereColor
    float sunDirX, sunDirY, sunDirZ, _pad2;             // sunDirection
    float sunColR, sunColG, sunColB, _pad3;              // sunColor
    float deepSpR, deepSpG, deepSpB, _pad4;              // deepSpaceColor
    float waterDeepR, waterDeepG, waterDeepB, _pad5;     // waterColorDeep
    float waterSurfR, waterSurfG, waterSurfB, _pad6;     // waterColorSurface
    float sandR, sandG, sandB, _pad7;                    // sandColor
    float treeR, treeG, treeB, _pad8;                    // treeColor
    float rockR, rockG, rockB, _pad9;                    // rockColor
    float iceR, iceG, iceB, _pad10;                      // iceColor
    float cloudColR, cloudColG, cloudColB, _pad11;       // cloudColor
    float sandLev, treeLev, rockLev, iceLev;             // biomeLevels
    float transition, _pad12, _pad13, _pad14;            // transitionPad

    // Black hole
    float isBlackHole, bhMass, bhAccretionInner, bhAccretionOuter; // bhParams1
    float bhDiskSpeed, bhDiskTurbulence, bhDiskBrightness, bhTempInner; // bhParams2
    float bhTempOuter, bhDopplerStrength, bhRaySteps, _pad15;          // bhParams3
    float bhDiskTintR, bhDiskTintG, bhDiskTintB, _pad16;              // bhDiskTint
};

static_assert(sizeof(PlanetUniformsVk) == 528,
    "PlanetUniformsVk size must match shader std140 layout (528 bytes)");

} // namespace astrocore
