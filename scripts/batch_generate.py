#!/usr/bin/env python3
"""
Batch generate planet render params using Claude Sonnet via AWS Bedrock.
Caches results to ~/.cache/astrosplat/planets/

Usage:
    python scripts/batch_generate.py --limit 100  # Generate first 100
    python scripts/batch_generate.py --all        # Generate all ~5000 confirmed exoplanets
    python scripts/batch_generate.py --parallel 10 # Run 10 parallel requests
"""

import argparse
import json
import os
import re
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from urllib.request import urlopen
from urllib.parse import quote

CACHE_DIR = Path.home() / ".cache" / "astrosplat" / "planets"
NASA_TAP_URL = "https://exoplanetarchive.ipac.caltech.edu/TAP/sync"

# System prompt - EXACT COPY from src/ai/PromptTemplates.hpp RENDER_PARAMS_SYSTEM_PROMPT
SYSTEM_PROMPT = """You are an expert at generating procedural planet rendering parameters. Given exoplanet data, generate a complete JSON object with rendering parameters to create a visually stunning and scientifically plausible visualization.

IMPORTANT: Respond ONLY with the JSON object. No explanations.

=== CRITICAL: STELLAR TYPE AFFECTS ALL COLORS ===

The host star's temperature determines the color of light illuminating the planet. YOU MUST adjust ALL colors accordingly:

- M-type RED DWARF (2500-3500K): Deep red/orange light. Atmosphere should be orange/red tinted, NOT blue. Surface colors warmer. Examples: Proxima Centauri, TRAPPIST-1
- K-type ORANGE DWARF (3500-5000K): Orange-tinted light. Warmer atmosphere colors. Examples: Alpha Centauri B
- G-type YELLOW (5000-6000K): White/yellow light like our Sun. Standard Earth-like colors work.
- F-type WHITE (6000-7500K): Slightly blue-white light. Cooler color tones acceptable.
- A-type BLUE-WHITE (7500-10000K): Blue-white light. Can have cooler blue-tinted colors.

For RED DWARF planets (very common!):
- atmosphereColor: Use warm oranges [0.8, 0.4, 0.2] NOT blue
- sunColor: [1.0, 0.5, 0.3] for deep red, [1.0, 0.7, 0.5] for warmer M-dwarf
- Surface colors (sand, rock, tree): Shift toward red/orange tones
- Ice/snow: Can have pinkish/orange tint from red starlight

Here are reference examples from our solar system (G-type yellow star):

=== EARTH-LIKE (temperate, ocean world, ~288K) ===
{
  "ambientLight": 0.005,
  "atmosphereColor": [0.4, 0.65, 1.0],
  "atmosphereDensity": 0.35,
  "bandingFrequency": 20.0,
  "bandingStrength": 0.0,
  "cloudAltitude": 0.25,
  "cloudColor": [1.0, 1.0, 1.0],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.0,
  "cloudsScale": 0.8,
  "cloudsSpeed": 1.5,
  "continentBlend": 0.5,
  "continentScale": 0.33,
  "craterStrength": 0.0,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.0,
  "fbmExponentiation": 4.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.5,
  "iceColor": [0.9, 0.93, 0.97],
  "iceLevel": 0.083,
  "noiseStrength": 0.04,
  "noiseType": 6,
  "polarCapSize": 0.12,
  "quality": 1.0,
  "radius": 1.0,
  "ridgedStrength": 0.0,
  "rockColor": [0.4, 0.35, 0.3],
  "rockLevel": 0.034,
  "rotationOffset": 0.6,
  "rotationSpeed": 0.1,
  "sandColor": [0.76, 0.7, 0.5],
  "sandLevel": 0.001,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 1.541,
  "terrainScale": 0.1,
  "transition": 0.005,
  "treeColor": [0.13, 0.35, 0.13],
  "treeLevel": 0.0,
  "waterColorDeep": [0.01, 0.03, 0.12],
  "waterColorSurface": [0.02, 0.08, 0.25],
  "waterLevel": 0.02
}

=== MARS-LIKE (cold desert, thin atmosphere, ~210K) ===
{
  "ambientLight": 0.01,
  "atmosphereColor": [0.43, 0.85, 1.0],
  "atmosphereDensity": 0.05,
  "bandingFrequency": 20.0,
  "bandingStrength": 0.0,
  "cloudAltitude": 0.25,
  "cloudColor": [1.0, 1.0, 1.0],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.015,
  "cloudsScale": 0.8,
  "cloudsSpeed": 1.5,
  "continentBlend": 0.15,
  "continentScale": 0.0,
  "craterStrength": 0.7,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.0,
  "fbmExponentiation": 8.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.6,
  "iceColor": [0.9, 0.85, 0.8],
  "iceLevel": 0.04,
  "noiseStrength": 0.103,
  "noiseType": 6,
  "polarCapSize": 0.173,
  "quality": 1.0,
  "radius": 0.532,
  "ridgedStrength": 0.2,
  "rockColor": [0.5, 0.3, 0.2],
  "rockLevel": 0.02,
  "rotationOffset": 0.6,
  "rotationSpeed": 0.1,
  "sandColor": [0.76, 0.5, 0.3],
  "sandLevel": 0.003,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 2.5,
  "terrainScale": 0.1,
  "transition": 0.01,
  "treeColor": [0.45, 0.21, 0.15],
  "treeLevel": 0.004,
  "waterColorDeep": [0.28, 0.05, 0.02],
  "waterColorSurface": [0.25, 0.12, 0.06],
  "waterLevel": 0.3
}

=== VENUS-LIKE (hot, thick atmosphere, ~737K) ===
{
  "ambientLight": 0.01,
  "atmosphereColor": [0.2, 0.07, 0.03],
  "atmosphereDensity": 1.0,
  "bandingFrequency": 5.0,
  "bandingStrength": 0.005,
  "cloudAltitude": 0.25,
  "cloudColor": [0.95, 0.85, 0.5],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.0,
  "cloudsScale": 1.5,
  "cloudsSpeed": 1.5,
  "continentBlend": 0.15,
  "continentScale": 1.0,
  "craterStrength": 0.0,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.0,
  "fbmExponentiation": 5.4,
  "fbmLacunarity": 1.96,
  "fbmOctaves": 8,
  "fbmPersistence": 0.9,
  "iceColor": [0.37, 0.2, 0.11],
  "iceLevel": 0.04,
  "noiseStrength": 0.225,
  "noiseType": 6,
  "polarCapSize": 0.15,
  "quality": 1.0,
  "radius": 0.949,
  "ridgedStrength": 0.0,
  "rockColor": [1.0, 1.0, 1.0],
  "rockLevel": 0.02,
  "rotationOffset": 0.6,
  "rotationSpeed": 0.054,
  "sandColor": [1.0, 1.0, 1.0],
  "sandLevel": 0.003,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 3.0,
  "terrainScale": 0.1,
  "transition": 0.01,
  "treeColor": [0.78, 0.5, 0.16],
  "treeLevel": 0.004,
  "waterColorDeep": [0.12, 0.04, 0.0],
  "waterColorSurface": [0.83, 0.37, 0.15],
  "waterLevel": 0.35
}

=== JUPITER-LIKE (gas giant, banded, ~165K) ===
{
  "ambientLight": 0.01,
  "atmosphereColor": [0.6, 0.4, 0.25],
  "atmosphereDensity": 0.5,
  "bandingFrequency": 30.0,
  "bandingStrength": 0.85,
  "cloudAltitude": 0.25,
  "cloudColor": [0.9, 0.85, 0.7],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.0,
  "cloudsScale": 2.5,
  "cloudsSpeed": 1.5,
  "continentBlend": 0.15,
  "continentScale": 1.0,
  "craterStrength": 0.0,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.4,
  "fbmExponentiation": 5.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.5,
  "iceColor": [0.95, 0.9, 0.8],
  "iceLevel": 0.04,
  "noiseStrength": 0.008,
  "noiseType": 0,
  "polarCapSize": 0.15,
  "quality": 1.0,
  "radius": 10.97,
  "ridgedStrength": 0.0,
  "rockColor": [0.28, 0.11, 0.0],
  "rockLevel": 0.02,
  "rotationOffset": 0.6,
  "rotationSpeed": 0.1,
  "sandColor": [0.85, 0.75, 0.6],
  "sandLevel": 0.003,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 3.0,
  "terrainScale": 0.8,
  "transition": 0.01,
  "treeColor": [0.8, 0.48, 0.24],
  "treeLevel": 0.004,
  "waterColorDeep": [0.01, 0.05, 0.15],
  "waterColorSurface": [0.02, 0.12, 0.27],
  "waterLevel": -1.0
}

=== NEPTUNE-LIKE (ice giant, blue, ~72K) ===
{
  "ambientLight": 0.01,
  "atmosphereColor": [0.2, 0.4, 0.85],
  "atmosphereDensity": 0.5,
  "bandingFrequency": 18.0,
  "bandingStrength": 0.75,
  "cloudAltitude": 0.25,
  "cloudColor": [0.5, 0.65, 0.9],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.02,
  "cloudsScale": 1.8,
  "cloudsSpeed": 1.5,
  "continentBlend": 0.15,
  "continentScale": 1.0,
  "craterStrength": 0.0,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.25,
  "fbmExponentiation": 5.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.5,
  "iceColor": [0.4, 0.55, 0.9],
  "iceLevel": 0.04,
  "noiseStrength": 0.005,
  "noiseType": 0,
  "polarCapSize": 0.15,
  "quality": 1.0,
  "radius": 3.86,
  "ridgedStrength": 0.0,
  "rockColor": [0.15, 0.3, 0.6],
  "rockLevel": 0.02,
  "rotationOffset": 0.6,
  "rotationSpeed": 0.1,
  "sandColor": [0.25, 0.4, 0.8],
  "sandLevel": 0.003,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 3.0,
  "terrainScale": 0.8,
  "transition": 0.01,
  "treeColor": [0.2, 0.35, 0.7],
  "treeLevel": 0.004,
  "waterColorDeep": [0.01, 0.05, 0.15],
  "waterColorSurface": [0.02, 0.12, 0.27],
  "waterLevel": -1.0
}

=== RED DWARF ROCKY (M-dwarf host ~3000K, rocky planet, ~280K) ===
Note: ALL colors shifted warm due to red starlight!
{
  "ambientLight": 0.008,
  "atmosphereColor": [0.85, 0.4, 0.2],
  "atmosphereDensity": 0.15,
  "bandingFrequency": 20.0,
  "bandingStrength": 0.0,
  "cloudAltitude": 0.25,
  "cloudColor": [0.95, 0.8, 0.7],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.01,
  "cloudsScale": 0.8,
  "cloudsSpeed": 1.5,
  "continentBlend": 0.15,
  "continentScale": 0.0,
  "craterStrength": 0.4,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.0,
  "fbmExponentiation": 4.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.55,
  "iceColor": [0.9, 0.75, 0.7],
  "iceLevel": 0.04,
  "noiseStrength": 0.06,
  "noiseType": 6,
  "polarCapSize": 0.1,
  "quality": 1.0,
  "radius": 1.0,
  "ridgedStrength": 0.15,
  "rockColor": [0.55, 0.35, 0.25],
  "rockLevel": 0.02,
  "rotationOffset": 0.6,
  "rotationSpeed": 0.0,
  "sandColor": [0.8, 0.5, 0.35],
  "sandLevel": 0.003,
  "sunColor": [1.0, 0.55, 0.35],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 2.5,
  "terrainScale": 0.1,
  "transition": 0.01,
  "treeColor": [0.5, 0.35, 0.25],
  "treeLevel": 0.004,
  "waterColorDeep": [0.15, 0.08, 0.05],
  "waterColorSurface": [0.35, 0.2, 0.15],
  "waterLevel": 0.02
}

=== MERCURY-LIKE (airless, cratered, ~440K day / 100K night) ===
{
  "ambientLight": 0.015,
  "atmosphereColor": [0.3, 0.3, 0.3],
  "atmosphereDensity": 0.0,
  "bandingFrequency": 5.0,
  "bandingStrength": 0.0,
  "cloudAltitude": 0.25,
  "cloudColor": [0.5, 0.5, 0.5],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.0,
  "cloudsScale": 0.8,
  "cloudsSpeed": 0.0,
  "continentBlend": 0.1,
  "continentScale": 0.0,
  "craterStrength": 0.85,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.0,
  "fbmExponentiation": 3.0,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.55,
  "iceColor": [0.55, 0.53, 0.5],
  "iceLevel": 0.1,
  "noiseStrength": 0.035,
  "noiseType": 6,
  "polarCapSize": 0.0,
  "quality": 1.0,
  "radius": 0.383,
  "ridgedStrength": 0.0,
  "rockColor": [0.25, 0.24, 0.22],
  "rockLevel": 0.02,
  "rotationOffset": 0.0,
  "rotationSpeed": 0.02,
  "sandColor": [0.45, 0.42, 0.4],
  "sandLevel": 0.005,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 4.0,
  "terrainScale": 0.15,
  "transition": 0.008,
  "treeColor": [0.35, 0.33, 0.3],
  "treeLevel": 0.01,
  "waterColorDeep": [0.2, 0.19, 0.18],
  "waterColorSurface": [0.3, 0.28, 0.26],
  "waterLevel": -0.5
}

=== SATURN-LIKE (gas giant, golden banded) ===
{
  "ambientLight": 0.01,
  "atmosphereColor": [0.8, 0.7, 0.4],
  "atmosphereDensity": 0.45,
  "bandingFrequency": 25.0,
  "bandingStrength": 0.9,
  "cloudAltitude": 0.25,
  "cloudColor": [0.95, 0.9, 0.75],
  "cloudThickness": 0.2,
  "cloudsDensity": 0.0,
  "cloudsScale": 2.0,
  "cloudsSpeed": 1.2,
  "continentBlend": 0.15,
  "continentScale": 1.0,
  "craterStrength": 0.0,
  "deepSpaceColor": [0.0, 0.0, 0.001],
  "domainWarpStrength": 0.3,
  "fbmExponentiation": 4.5,
  "fbmLacunarity": 2.0,
  "fbmOctaves": 6,
  "fbmPersistence": 0.5,
  "iceColor": [0.95, 0.92, 0.8],
  "iceLevel": 0.04,
  "noiseStrength": 0.006,
  "noiseType": 0,
  "polarCapSize": 0.15,
  "quality": 1.0,
  "radius": 9.14,
  "ridgedStrength": 0.0,
  "rockColor": [0.7, 0.6, 0.45],
  "rockLevel": 0.02,
  "rotationOffset": 0.0,
  "rotationSpeed": 0.12,
  "sandColor": [0.9, 0.85, 0.65],
  "sandLevel": 0.003,
  "sunColor": [1.0, 1.0, 0.9],
  "sunDirection": [1.0, 1.0, 0.5],
  "sunIntensity": 2.5,
  "terrainScale": 0.8,
  "transition": 0.01,
  "treeColor": [0.8, 0.7, 0.5],
  "treeLevel": 0.004,
  "waterColorDeep": [0.01, 0.05, 0.15],
  "waterColorSurface": [0.02, 0.12, 0.27],
  "waterLevel": -1.0
}

KEY PARAMETERS EXPLAINED:
- radius: Planet size in Earth radii
- noiseStrength: Terrain height variation (0.001-0.3)
- atmosphereDensity: 0 = none, 1 = very thick
- bandingStrength: 0 = no bands (rocky), 0.7-1.0 = gas giant bands (USE HIGH VALUES for visibility!)
- cloudsDensity: MAXIMUM 0.02! Higher values look bad. Use 0.0-0.02 range only.
- waterLevel: < 0 = no water, 0-0.3 = ocean coverage
- craterStrength: 0 = smooth, 0.7-1.0 = heavily cratered
- All colors are RGB arrays [0-1, 0-1, 0-1]

CRITICAL COLOR RULES:
1. ICE COLOR: iceColor should be a LIGHTER/PALER TINT of the planet's dominant color!
   - Red planet -> iceColor should be pale pink/salmon [0.9, 0.7, 0.7], NOT white
   - Brown planet -> iceColor should be tan/cream [0.9, 0.85, 0.75], NOT white
   - Blue planet -> iceColor can be pale blue/white [0.85, 0.9, 1.0]
   - Orange planet -> iceColor should be pale peach [0.95, 0.85, 0.7]
   - Ice is the HIGHEST altitude terrain, so it must complement the overall look!

2. BANDING: When bandingStrength > 0, set it to at least 0.7 for VISIBLE bands.
   Gas giants MUST have prominent, clearly visible bands. Don't use subtle values.

3. CLOUDS: Keep cloudsDensity at 0.0-0.02. Any higher obscures planet features.

VISUAL STYLE - IMPORTANT:
Create VIBRANT, SATURATED, VISUALLY STRIKING planets. Avoid bland, washed-out, or grey colors.
- Use RICH, DEEP colors for oceans (deep blues, teals, or alien purples/greens)
- Use VIVID colors for atmospheres (bright blues, oranges, purples - not grey!)
- Surface colors should have CHARACTER - rust reds, golden yellows, deep greens, not muddy browns
- Gas giants should have BOLD contrast between bands - USE HIGH bandingStrength (0.8-1.0)
- Lava worlds should GLOW with intense oranges and reds
- sunIntensity should be 2.5-4.0 for dramatic lighting
- Avoid grey, beige, or desaturated colors unless specifically appropriate (like Mercury)
- FEATURES MUST BE VISIBLE: Emphasize terrain, bands, and color variations

GUIDELINES:
- Hot planets (>500K): INTENSE orange/red/yellow atmospheres and surfaces
- Cold planets (<200K): CRISP blue/white/cyan ice and atmosphere
- Gas giants (radius > 5): BOLD banding with high contrast, vivid atmosphere colors
- Rocky (radius < 2): Rich terrain colors - rust, ochre, tan - not grey
- Habitable zone: LUSH greens, deep blue oceans, white clouds
- Ocean worlds: DEEP blue/teal waters, NOT pale or grey
- Desert worlds: GOLDEN sands, orange skies, dramatic
- Airless: Can be greyer but still have interesting color variation
- Match the radius to the actual planet size in Earth radii"""


def get_all_exoplanets(limit=None):
    """Fetch exoplanet list from NASA TAP API"""
    query = """SELECT pl_name, pl_bmasse, pl_rade, pl_dens, pl_eqt, pl_insol,
                      st_teff, st_spectype, st_met, hostname
               FROM ps WHERE default_flag = 1"""
    if limit:
        query = f"SELECT TOP {limit} " + query[7:]
    query += " ORDER BY pl_name"

    url = f"{NASA_TAP_URL}?query={quote(query)}&format=json"
    print(f"Fetching exoplanets from NASA...")

    with urlopen(url, timeout=60) as response:
        data = json.loads(response.read())

    print(f"Found {len(data)} exoplanets")
    return data


def classify_planet(planet):
    """Classify planet type from mass/radius/density - matches C++ ExoplanetConverter::inferPlanetType"""
    mass = planet.get('pl_bmasse') or 0
    radius = planet.get('pl_rade') or 0
    density = planet.get('pl_dens') or 0
    temp = planet.get('pl_eqt') or 0
    insolation = planet.get('pl_insol') or 0
    stellar_met = planet.get('st_met') or 0

    # Calculate density if we have mass + radius
    if density == 0 and mass > 0 and radius > 0:
        density = 5.51 * (mass / (radius ** 3))

    # Estimate temperature from insolation
    if temp == 0 and insolation > 0:
        temp = 278.0 * (insolation ** 0.25)

    # 1. DEFINITE GAS GIANTS
    if mass > 100 or radius > 8.0:
        return "Hot Jupiter" if temp > 1000 else "Gas Giant"

    # 2. DENSITY-BASED classification (most reliable)
    if density > 0:
        if density < 2.0:
            if mass > 50 or radius > 6.0:
                return "Gas Giant"
            return "Neptune-like"
        if density < 4.0:
            if mass > 5 or radius > 2.5:
                return "Neptune-like"
            if 0 < temp < 350:
                return "Ocean World"
            return "Super-Earth"
        # density >= 4.0 = rocky
        if temp > 1500:
            return "Lava World"
        if mass > 1.5 or radius > 1.25:
            return "Super-Earth"
        return "Terrestrial"

    # 3. MASS-BASED (for RV planets without radius)
    if mass > 0 and radius == 0:
        if mass > 50:
            return "Gas Giant"
        if mass > 5:
            return "Neptune-like"
        if mass > 2 and stellar_met > 0.1:
            return "Neptune-like"
        if mass > 1.5:
            return "Super-Earth"
        if temp > 1500:
            return "Lava World"
        return "Terrestrial"

    # 4. RADIUS-BASED (for transit planets)
    if radius > 0:
        if radius > 6.0:
            return "Gas Giant"
        if radius > 2.0:
            return "Neptune-like"
        if radius > 1.6:
            return "Neptune-like" if temp < 1000 else "Super-Earth"
        if radius > 1.25:
            return "Super-Earth"
        return "Terrestrial"

    # 5. TEMPERATURE-BASED fallback
    if temp > 1500:
        return "Lava World"

    return "Unknown"


def build_user_prompt(planet):
    """Build user prompt with planet data - matches C++ buildFullRenderParamsPrompt"""
    name = planet.get('pl_name', 'Unknown')
    hostname = planet.get('hostname', 'Unknown')

    mass = planet.get('pl_bmasse') or 0
    radius = planet.get('pl_rade') or 0
    density = planet.get('pl_dens') or 0
    temp = planet.get('pl_eqt') or 0
    insolation = planet.get('pl_insol') or 0
    star_temp = planet.get('st_teff') or 0
    star_spec = planet.get('st_spectype') or ''
    stellar_met = planet.get('st_met') or 0

    # Calculate density if missing
    if density == 0 and mass > 0 and radius > 0:
        density = 5.51 * (mass / (radius ** 3))

    # Estimate temp from insolation
    if temp == 0 and insolation > 0:
        temp = 278.0 * (insolation ** 0.25)

    planet_type = classify_planet(planet)

    prompt = "Generate complete rendering parameters for this exoplanet:\n\n"
    prompt += "=== EXOPLANET DATA ===\n"
    prompt += f"Name: {name}\n"
    prompt += f"\n*** PLANET TYPE: {planet_type} ***\n\n"

    if mass > 0:
        prompt += f"Mass: {mass:.3f} Earth masses"
        if mass > 318:
            prompt += f" ({mass / 318.0:.2f} Jupiter masses)"
        prompt += "\n"

    if radius > 0:
        prompt += f"Radius: {radius:.3f} Earth radii"
        if radius > 11:
            prompt += f" ({radius / 11.2:.2f} Jupiter radii)"
        prompt += "\n"

    if temp > 0:
        prompt += f"Equilibrium Temperature: {temp:.0f} K\n"

    if density > 0:
        prompt += f"Density: {density:.2f} g/cm3\n"

    if insolation > 0:
        prompt += f"Insolation Flux: {insolation:.2f} Earth flux\n"

    # Host star info
    prompt += f"\n=== HOST STAR ===\n"
    prompt += f"Star Name: {hostname}\n"

    if star_spec:
        prompt += f"Spectral Type: {star_spec}\n"

    if star_temp > 0:
        prompt += f"Star Temperature: {star_temp:.0f} K\n"

        # Add explicit color guidance for non-Sun-like stars
        if star_temp < 4000:
            prompt += "\n" + "=" * 66 + "\n"
            prompt += "  *** RED DWARF HOST - MANDATORY COLOR SHIFT ***\n"
            prompt += "=" * 66 + "\n"
            prompt += f"This is an M-type RED DWARF ({star_temp:.0f}K) - DEEP RED/ORANGE starlight!\n"
            prompt += "YOU MUST USE WARM/RED-TINTED COLORS:\n"
            prompt += "  - atmosphereColor: [0.8-0.9, 0.3-0.5, 0.15-0.3] (orange/red, NOT blue!)\n"
            prompt += "  - sunColor: [1.0, 0.5, 0.3] (deep red starlight)\n"
            prompt += "  - All surface colors shifted toward red/orange tones\n"
            prompt += "  - Ice/clouds: pinkish/orange tint, not pure white\n"
            prompt += "  - NO BLUE ATMOSPHERE - blue scattering doesn't work with red light!\n\n"
        elif star_temp < 5000:
            prompt += f"\nThis is a K-type ORANGE DWARF ({star_temp:.0f}K) - warmer orange-tinted light\n"
            prompt += "  - Use slightly warmer atmosphere colors than Earth\n"
            prompt += "  - sunColor: [1.0, 0.8, 0.6]\n\n"

    if stellar_met != 0:
        prompt += f"Metallicity [Fe/H]: {stellar_met:.2f}\n"

    prompt += "\nGenerate the JSON render parameters now."

    return prompt


def call_bedrock(system_prompt, user_prompt):
    """Call AWS Bedrock with Claude Sonnet using temp files (avoids command line length limits)"""
    import tempfile

    payload = {
        "anthropic_version": "bedrock-2023-05-31",
        "max_tokens": 2000,
        "temperature": 0,
        "messages": [
            {"role": "user", "content": f"{system_prompt}\n\n{user_prompt}"}
        ]
    }

    # Write payload to temp file to avoid command line length limits
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
        json.dump(payload, f)
        request_file = f.name

    output_file = tempfile.mktemp(suffix='.json')

    try:
        model_id = os.environ.get("BEDROCK_MODEL_ID", "us.anthropic.claude-sonnet-4-5-20250929-v1:0")
        region = os.environ.get("AWS_REGION", "us-east-1")

        cmd = [
            "aws", "bedrock-runtime", "invoke-model",
            "--model-id", model_id,
            "--region", region,
            "--body", f"fileb://{request_file}",
            "--content-type", "application/json",
            "--accept", "application/json",
            output_file
        ]

        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        if result.returncode != 0:
            raise Exception(f"Bedrock error: {result.stderr}")

        # Read response from output file
        with open(output_file, 'r') as f:
            response = json.load(f)

        text = response['content'][0]['text']

        # Extract JSON from response (handle markdown code blocks)
        if '```json' in text:
            match = re.search(r'```json\s*(.*?)\s*```', text, re.DOTALL)
            if match:
                text = match.group(1)
        elif '```' in text:
            match = re.search(r'```\s*(.*?)\s*```', text, re.DOTALL)
            if match:
                text = match.group(1)

        # Find JSON object boundaries
        start = text.find('{')
        end = text.rfind('}')
        if start != -1 and end != -1:
            text = text[start:end + 1]

        return json.loads(text.strip())

    finally:
        # Clean up temp files
        if os.path.exists(request_file):
            os.remove(request_file)
        if os.path.exists(output_file):
            os.remove(output_file)


def sanitize_filename(name):
    """Convert planet name to safe filename"""
    return re.sub(r'[^\w\-]', '_', name.lower())


def process_planet(planet, force=False, max_retries=3):
    """Generate and cache params for a single planet with retry logic"""
    name = planet.get('pl_name', 'Unknown')
    filename = sanitize_filename(name) + ".json"
    cache_path = CACHE_DIR / filename

    # Skip if already cached
    if cache_path.exists() and not force:
        return (name, "cached", None)

    last_error = None
    for attempt in range(max_retries):
        try:
            user_prompt = build_user_prompt(planet)
            params = call_bedrock(SYSTEM_PROMPT, user_prompt)

            # Add metadata
            params['_planet_name'] = name
            params['_planet_type'] = classify_planet(planet)
            params['_generated_by'] = 'claude-sonnet-4.5'
            params['_timestamp'] = time.strftime('%Y-%m-%d %H:%M:%S')

            # Save to cache
            cache_path.write_text(json.dumps(params, indent=2))

            return (name, "generated", None)
        except Exception as e:
            last_error = str(e)
            if attempt < max_retries - 1:
                # Exponential backoff: 2s, 4s, 8s
                wait_time = 2 ** (attempt + 1)
                time.sleep(wait_time)

    return (name, "error", f"Failed after {max_retries} retries: {last_error}")


def main():
    parser = argparse.ArgumentParser(description='Batch generate planet render params')
    parser.add_argument('--limit', type=int, help='Limit number of planets')
    parser.add_argument('--all', action='store_true', help='Process all planets')
    parser.add_argument('--parallel', type=int, default=5, help='Parallel requests (default: 5)')
    parser.add_argument('--force', action='store_true', help='Regenerate even if cached')
    args = parser.parse_args()

    # Create cache directory
    CACHE_DIR.mkdir(parents=True, exist_ok=True)

    # Get planets
    limit = None if args.all else (args.limit or 100)
    planets = get_all_exoplanets(limit)

    # Process in parallel
    generated = 0
    cached = 0
    errors = 0

    print(f"\nProcessing {len(planets)} planets with {args.parallel} parallel workers...")

    with ThreadPoolExecutor(max_workers=args.parallel) as executor:
        futures = {executor.submit(process_planet, p, args.force): p for p in planets}

        for i, future in enumerate(as_completed(futures), 1):
            name, status, error = future.result()

            if status == "generated":
                generated += 1
                print(f"[{i}/{len(planets)}] Generated: {name}")
            elif status == "cached":
                cached += 1
                if i % 100 == 0:
                    print(f"[{i}/{len(planets)}] Skipped {cached} cached...")
            else:
                errors += 1
                print(f"[{i}/{len(planets)}] ERROR: {name} - {error}")

            # Rate limiting (Bedrock has limits)
            if status == "generated":
                time.sleep(0.5)  # 2 requests per second max

    print(f"\nDone! Generated: {generated}, Cached: {cached}, Errors: {errors}")
    print(f"Cache location: {CACHE_DIR}")


if __name__ == "__main__":
    main()
