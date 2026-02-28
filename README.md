# AstroSplat

Real-time procedural exoplanet renderer. Queries NASA's exoplanet archive, derives visual parameters from physics (mass, radius, temperature, density), optionally augments them with Claude via AWS Bedrock, and raymarches the result on the GPU in a single fragment shader. No meshes, no textures -- everything is procedural.

There's also a companion star explorer that renders ~176 million Gaia DR3 stars in a navigable 3D octree.

![Vulkan](https://img.shields.io/badge/Vulkan-1.2+-red) ![C++23](https://img.shields.io/badge/C%2B%2B-23-blue) ![CMake](https://img.shields.io/badge/CMake-3.25+-green)

## What it does

Type a planet name. If it's in our solar system, we already know the ground-truth parameters. If it's an exoplanet, we pull whatever the archive has measured -- mass, radius, equilibrium temperature, orbital period, host star properties -- and derive the rest. Surface gravity gives you atmospheric density. Temperature drives ice caps and water level. Density hints at composition. The gaps get filled by physics-based heuristics and, if you have AWS credentials, by a Claude inference pass that respects measured data and only fills empty slots.

The fragment shader does the heavy lifting: FBM noise for terrain, ridged noise for mountain ranges, craters, tectonic plate boundaries, volumetric clouds with self-shadowing, Rayleigh atmospheric scattering, biome transitions (water / sand / vegetation / rock / ice), and optionally black hole accretion disks with Doppler shifting and gravitational lensing.

The star explorer is a separate binary. It ingests Gaia DR3 data into a spatial octree and lets you fly through the local stellar neighbourhood with LOD-based rendering.

## Building

### Dependencies

You need these on your system before building:

| Dependency | Version | Notes |
|---|---|---|
| **CMake** | 3.25+ | Build system |
| **Vulkan SDK** | 1.2+ | Must include `glslc` for SPIR-V shader compilation |
| **C++ compiler** | C++23 support | GCC 13+, Clang 16+, or MSVC 17.6+ (VS 2022) |
| **libcurl** | any | Optional -- built from source if not found |

Everything else (GLFW, GLM, Dear ImGui, vk-bootstrap, VMA, spdlog, pugixml, Catch2) is pulled automatically via CMake FetchContent. First build takes a few minutes while it downloads and compiles these.

### Platform-specific setup

**Linux (Ubuntu/Debian)**
```bash
sudo apt install cmake vulkan-sdk libvulkan-dev glslc libcurl4-openssl-dev
```

On Fedora:
```bash
sudo dnf install cmake vulkan-loader-devel vulkan-tools glslc libcurl-devel
```

On Arch:
```bash
sudo pacman -S cmake vulkan-devel vulkan-tools shaderc curl
```

**macOS**

Install the [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home) or get it through Homebrew:
```bash
brew install cmake vulkan-sdk molten-vk curl
```

MoltenVK translates Vulkan calls to Metal under the hood. The build system auto-detects the MoltenVK ICD path from `/opt/homebrew` or `/usr/local`.

**Windows**

1. Install the [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home) -- make sure `glslc.exe` is on your PATH
2. Install [CMake](https://cmake.org/download/) 3.25+
3. Use Visual Studio 2022 (17.6+) or install MinGW-w64 with GCC 13+

### Build commands

Using the Makefile (Linux/macOS):
```bash
make                # Release build
make debug          # Debug build
make asan           # Debug + AddressSanitizer
make clean          # Nuke build/
make rebuild        # clean + release
make run            # Build and launch astrosplat
make explorer       # Build and launch starexplorer
make test           # Build and run tests
```

Or invoke CMake directly (all platforms):
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

On Windows with MSVC:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

For ARM64 (aarch64) cross-compilation on Windows:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A ARM64
cmake --build build --config Release
```

On Apple Silicon, native ARM64 builds happen automatically. On Linux aarch64, just build normally -- CMake picks up the host architecture.

### Build outputs

After a successful build:
```
build/astrosplat          # Main planet renderer
build/starexplorer        # 3D star map navigator
build/shaders/*.spv       # Compiled SPIR-V shaders
```

## Running

```bash
./build/astrosplat
```

The UI opens with a default procedural planet. Type any planet name in the search bar:
- Solar system planets load instantly from ground-truth data (Earth, Mars, Jupiter, etc.)
- Exoplanets (Kepler-452b, TRAPPIST-1e, HD 209458 b, ...) query NASA TAP and run the parameter derivation pipeline

### AI inference (optional)

If you want Claude to fill in visual parameters that pure physics can't determine (surface colours, cloud patterns, terrain character):

```bash
export AWS_ACCESS_KEY_ID=...
export AWS_SECRET_ACCESS_KEY=...
export AWS_DEFAULT_REGION=us-east-1
```

You need Bedrock model access enabled for Claude in your AWS account. Without credentials, the app still works -- it just falls back to physics-only derivation and solar system analog matching.

### Star explorer

```bash
./build/starexplorer
```

Requires a pre-built star octree. Generate one from Gaia DR3 data:
```bash
pip install -r scripts/requirements.txt
python scripts/gen_star_octree.py
```

This processes ~176 million stars into a spatial octree for LOD rendering. Takes a while and a decent amount of RAM.

## Project layout

```
src/
  core/           Application lifecycle, windowing (GLFW), logging (spdlog)
  render/         Vulkan renderer, camera, procedural planet parameters
  ui/             Dear ImGui overlay -- parameter sliders, planet search
  data/           NASA TAP, Open Exoplanet Catalog, Gaia DR3, CDS clients
  ai/             AWS Bedrock inference, prompt templates
  intro/          Startup particle animation
  explorer/       Star explorer (separate Vulkan renderer + octree)
  physics/        N-body simulation, Barnes-Hut octree, integrators
  simulation/     Simulation orchestrator bridging physics and rendering
shaders/          GLSL 450 shaders compiled to SPIR-V
assets/           Precomputed noise textures, starmap cubemap faces
src_ml/           PyTorch BERT-based parameter imputation (training pipeline)
scripts/          Star octree + cubemap generation from Gaia DR3
tests/            Catch2 unit tests for data layer and rendering
cmake/            Build modules (compiler flags, FetchContent deps, shader compilation)
```

## ML pipeline

There's a secondary ML approach alongside the Claude inference: a BERT-based model trained on NASA archive data to predict missing planet parameters from physical observables. See `BERT_PLAN.md` for the training pipeline and `src_ml/` for the PyTorch code.

The trained checkpoint lives at `checkpoints/bert_imputer_best.pt`.

## Shader architecture

The planet renderer is a single full-screen quad. The fragment shader (`shaders/planet_vk.frag`) does all the work:

- Ray-sphere intersection to find the planet surface
- FBM noise with configurable persistence/lacunarity/octaves for terrain heightmap
- Ridged noise for mountain ranges, crater noise for impact basins
- Biome mapping based on elevation + latitude (water, sand, vegetation, rock, ice)
- Volumetric cloud layer with density-based self-shadowing
- Rayleigh scattering for atmospheric haze
- Optional black hole mode: accretion disk, gravitational lensing, Doppler shift

All parameters are packed into a single std140 uniform buffer (~560 bytes) and updated per-frame.

## License

This is a hackathon/research project. If you're doing something cool with procedural planet rendering, feel free to poke around.
