# AkRender

A graphics learning and algorithm experimentation framework built with **Slang**
shaders and **Vulkan**.

| CMake target | Role |
|--------------|------|
| `AkRender::Core` | Core library — Vulkan post-init helpers (Vulkan + VMA) |
| `AkRender::AkRender` | Optional bootstrap framework — windowing / platform glue |
| `AkRenderShaderSet::…` | Independent shader build system (see below) |

Using `AkRender::Core` does not require the framework target.

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
| GLFW3 | Windowing & input (framework only) |
| GLM | Math library (framework only) |
| spdlog | Logging (framework only) |
| stb | Image loading (framework only) |
| Slang | Shader language & compiler (ShaderSet) |
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

Core only (skip the bootstrap framework):

```bash
cmake --preset debug -DAKRENDER_BUILD_FRAMEWORK=OFF
cmake --build --preset debug
```

Link targets:

```cmake
find_package(AkRender REQUIRED)
target_link_libraries(my_app PRIVATE AkRender::Core)

# Optional bootstrap framework:
find_package(AkRender REQUIRED COMPONENTS Framework)
target_link_libraries(my_app PRIVATE AkRender::AkRender)
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
