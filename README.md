# AkRender

A toy renderer built with **Slang** shaders and **Vulkan** API.

## Prerequisites

- **CMake** ≥ 3.21
- **vcpkg** (with `VCPKG_ROOT` environment variable set)
- **Vulkan SDK** (≥ 1.3)
- C++20 compiler (GCC ≥ 11, Clang ≥ 14, MSVC ≥ 2022)

## Dependencies (managed via vcpkg)

| Library | Purpose |
|---------|---------|
| Vulkan | Graphics API |
| VulkanMemoryAllocator | GPU memory allocation |
| GLFW3 | Windowing & input |
| GLM | Math library |
| spdlog | Logging |
| stb | Image loading |
| Slang | Shader language & compiler |

## Build

```bash
# Configure (Debug)
cmake --preset debug

# Build
cmake --build --preset debug

# Or Release
cmake --preset release
cmake --build --preset release
```

Shaders in `shaders/` are automatically compiled to SPIR-V during the build.

## Project Structure

```
AkRender/
├── CMakeLists.txt          # Top-level CMake
├── CMakePresets.json       # Build presets (debug / release)
├── vcpkg.json              # vcpkg manifest
├── src/
│   ├── CMakeLists.txt
│   └── main.cpp            # Entry point
└── shaders/
    └── triangle.slang      # Example Slang shader
```
