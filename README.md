# AkRender

A toy renderer built with **Slang** shaders and **Vulkan** API.

## Prerequisites

- **CMake** ≥ 3.21
- **vcpkg** (with `VCPKG_ROOT` environment variable set)
- **Vulkan SDK** (≥ 1.3)
- C++23 compiler (GCC ≥ 11, Clang ≥ 14, MSVC ≥ 2022)

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
| inja | ShaderSet code generation templates |
| CLI11 | ShaderSet generator CLI |
| GTest | Unit tests |

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

Shader compilation and embedding are handled by **ShaderSet** (`tools/ShaderSet`).
See [tools/ShaderSet/README.md](tools/ShaderSet/README.md) and
[docs/shader-build-system-design.md](docs/shader-build-system-design.md) for details.

## ShaderSet (shader build system)

Shader manifests are written in C++ and compiled into embedded shader sets at build time:

```cmake
find_package(AkRenderShaderSet REQUIRED)

add_shader_set(my_shaders path/to/manifest.cpp)
target_link_libraries(my_app PRIVATE my_shaders AkRenderShaderSet::AkRenderShaderSet)
```

Example manifests live under `tools/ShaderSet/tests/`:

- `GeneratorExample/` — file embedding and VFS layout
- `ShaderCompileExample/` — Slang module + SPIR-V/IR build-time compilation
