# AkRender 着色器构建系统设计方案（v2）

> **核心思路**：避开 CMake 糟糕的脚本语法，用 C++ 编写着色器配置。
> 用户写一个 C++ 源文件来描述着色器集，CMake 编译并执行这个文件，
> 生成 CMake 片段 + C++ 注册表，最终产出正常的可链接库目标。

---

## 1. 架构总览：双层 DSL

```
┌──────────────────────────────────────────────────────────────────────────┐
│  第 1 层：C++ 配置（用户编写）                                            │
│                                                                          │
│  shaders.config.cpp                                                      │
│  ┌──────────────────────────────────────────────────────┐                │
│  │ int main() {                                          │                │
│  │   ShaderProject project("my_game");                    │                │
│  │                                                        │                │
│  │   project.add_shader("triangle_vert")                  │                │
│  │       .source("shaders/triangle.vert")                 │                │
│  │       .type(SourceType::GLSL).stage(Stage::Vertex);    │                │
│  │                                                        │                │
│  │   project.add_shader("compute_main")                   │                │
│  │       .source("shaders/compute.slang")                 │                │
│  │       .type(SourceType::Slang).stage(Stage::Compute)   │                │
│  │       .entry("main").link_module("math");              │                │
│  │                                                        │                │
│  │   project.embed_into("my_app");  // or deploy("shaders");             │
│  │   project.generate("${GEN_DIR}");                                      │
│  │ }                                                        │                │
│  └──────────────────────────────────────────────────────┘                │
└──────────────────────────────────────────────────────┬───────────────────┘
                                                        │ 编译并执行
                                                        ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  第 2 层：生成产物（由 C++ 配置工具自动产出）                            │
│                                                                          │
│  ${GEN_DIR}/                                                            │
│  ├── shaders.cmake         ← CMake 片段（自定义目标、编译规则）          │
│  ├── shader_registry.hpp   ← C++ 索引枚举 + ShaderBlob 声明             │
│  ├── shader_registry.cpp   ← C++ 嵌入数据（嵌入模式）                   │
│  └── shader_paths.hpp      ← C++ 路径表（部署模式）                     │
└──────────────────────────────────────────────────────┬───────────────────┘
                                                        │ CMake include()
                                                        ▼
┌──────────────────────────────────────────────────────────────────────────┐
│  第 3 层：CMake 消费（轻量编排）                                         │
│                                                                          │
│  CMakeLists.txt:                                                         │
│    # 1. 编译并运行 C++ 配置工具                                          │
│    # 2. include() 生成的 shaders.cmake                                   │
│    # 3. 得到一个正常的 INTERFACE/SHARED 库目标                           │
│    # 4. target_link_libraries(my_app my_shaders)                         │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. C++ 配置工具（Shader Configurator）

### 2.1 工具自身结构

```cpp
// tools/ShaderConfigurator/include/AkRender/ShaderConfigurator/Project.hpp

namespace AkRender::ShaderConfigurator {

    // ── 着色器来源类型 ────────────────────────────────────────────
    enum class SourceType {
        SpirV,          // 预编译 SPIR-V
        GLSL,           // GLSL 源码 → 构建时编译
        HLSL,           // HLSL 源码 → 构建时编译
        Slang,          // Slang 源码 → 构建时编译
        SlangJIT,       // Slang 源码 → 运行时编译
    };

    // ── 着色器阶段 ────────────────────────────────────────────────
    enum class Stage { Vertex, Fragment, Compute, Geometry,
                       TessControl, TessEval, Mesh, Task, ... };

    // ── 编译目标格式 ──────────────────────────────────────────────
    enum class TargetFormat { SpirV, DXIL, WGSL };

    // ── 消费策略 ──────────────────────────────────────────────────
    enum class Consumption {
        Embedded,    // 嵌入到目标中（生成字节数组）
        Deployed,    // 部署为外部文件（生成路径表）
    };

    // ── 单个着色器声明（fluent builder）───────────────────────────
    class ShaderDecl {
    public:
        ShaderDecl& source(std::filesystem::path path);
        ShaderDecl& type(SourceType t);
        ShaderDecl& stage(Stage s);
        ShaderDecl& entry(std::string_view name);
        ShaderDecl& target_format(TargetFormat fmt);
        ShaderDecl& link_module(std::string_view module_name);
        ShaderDecl& define(std::string_view macro, std::string_view value = {});
        ShaderDecl& option(std::string_view opt);
    };

    // ── 着色器项目 ────────────────────────────────────────────────
    class ShaderProject {
    public:
        explicit ShaderProject(std::string_view name);

        // 添加着色器（返回引用以继续 builder 链）
        ShaderDecl& add_shader(std::string_view name);

        // 添加 Slang 模块（供 import 使用）
        ShaderProject& add_module(std::string_view name,
                                   std::initializer_list<std::string> sources);

        // 设置消费策略
        ShaderProject& embed_into(std::string_view target_name);
        ShaderProject& deploy_to(std::string_view relative_path);

        // 指定生成输出目录 + 执行生成
        // 由 CMake 在 ${CMAKE_CURRENT_BINARY_DIR}/shader-gen 上调用
        void generate(const std::filesystem::path& output_dir);
    };
}
```

### 2.2 用户编写的配置文件示例

```cpp
// my_game/shaders.config.cpp
#include <AkRender/ShaderConfigurator/Project.hpp>
#include <filesystem>

namespace akr = AkRender::ShaderConfigurator;

int main(int argc, char* argv[]) {
    // argv[1] = output directory（由 CMake 传入）
    if (argc < 2) return 1;
    std::filesystem::path gen_dir = argv[1];

    // ── 创建项目 ──────────────────────────────────────────────────
    akr::ShaderProject project("game_shaders");

    // ── 注册 Slang 模块 ──────────────────────────────────────────
    project.add_module("math_utils", {"shaders/math_utils.slang"});
    project.add_module("noise",      {"shaders/noise.slang",
                                       "shaders/random.slang"});

    // ── 普通着色器 ────────────────────────────────────────────────
    project.add_shader("triangle_vert")
        .source("shaders/triangle.vert")
        .type(akr::SourceType::GLSL)
        .stage(akr::Stage::Vertex);

    project.add_shader("triangle_frag")
        .source("shaders/triangle.frag")
        .type(akr::SourceType::GLSL)
        .stage(akr::Stage::Fragment)
        .define("MAX_LIGHTS", "8");

    // ── Slang 着色器（构建时编译到 SPIR-V）───────────────────────
    project.add_shader("compute_main")
        .source("shaders/compute.slang")
        .type(akr::SourceType::Slang)
        .stage(akr::Stage::Compute)
        .entry("main")
        .target_format(akr::TargetFormat::SpirV)
        .link_module("math_utils")
        .link_module("noise");

    // ── 预编译 SPIR-V ─────────────────────────────────────────────
    project.add_shader("prebuilt_effect")
        .source("shaders/effect.spv")
        .type(akr::SourceType::SpirV);

    // ── JIT 着色器（仅嵌入源码，运行时编译）─────────────────────
    project.add_shader("runtime_fx")
        .source("shaders/fx.slang")
        .type(akr::SourceType::SlangJIT)
        .link_module("math_utils");

    // ── 消费策略：嵌入到 my_app 目标中 ───────────────────────────
    project.embed_into("my_app");

    // ── 生成所有产出 ──────────────────────────────────────────────
    project.generate(gen_dir);
    return 0;
}
```

---

## 3. 生成产物

C++ 配置工具执行后，在 `gen_dir` 下产出三类文件：

### 3.1 生成的 CMake 片段（`shaders.cmake`）

```cmake
# 自动生成 —— 由 shaders.config.cpp 生成。请勿手动编辑。

# ── 着色器集：game_shaders ──────────────────────────────────────────
# 包含 5 个着色器: triangle_vert, triangle_frag, compute_main,
#                  prebuilt_effect, runtime_fx

# ── 工具检测 ────────────────────────────────────────────────────────
find_program(_GLSLANGVALIDATOR glslangValidator)
find_program(_DXC dxc)

# ── 编译输出目录 ────────────────────────────────────────────────────
set(_SHADER_OUT "${CMAKE_CURRENT_BINARY_DIR}/shader-bin/game_shaders")

# ── Slang 模块编译 ──────────────────────────────────────────────────
add_custom_command(
    OUTPUT "${_SHADER_OUT}/modules/math_utils.slang-module"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_SHADER_OUT}/modules"
    COMMAND slangc "shaders/math_utils.slang"
        -emit-ir -module-name math_utils
        -o "${_SHADER_OUT}/modules/math_utils.slang-module"
    DEPENDS "shaders/math_utils.slang"
    COMMENT "Slang module: math_utils"
)

# ── 着色器编译（GLSL）─────────────────────────────────────────────
add_custom_command(
    OUTPUT "${_SHADER_OUT}/triangle_vert.spv"
    COMMAND ${_GLSLANGVALIDATOR}
        -V "shaders/triangle.vert"
        -o "${_SHADER_OUT}/triangle_vert.spv"
        -S vert
    DEPENDS "shaders/triangle.vert"
    COMMENT "GLSL→SPIR-V: triangle_vert"
)

# ── 着色器编译（Slang）─────────────────────────────────────────────
add_custom_command(
    OUTPUT "${_SHADER_OUT}/compute_main.spv"
    COMMAND slangc "shaders/compute.slang"
        -target spirv -matrix-layout-row-major -O2
        -I "${_SHADER_OUT}/modules"
        -o "${_SHADER_OUT}/compute_main.spv"
        -depfile "${_SHADER_OUT}/compute_main.d"
    DEPENDS "shaders/compute.slang"
            "${_SHADER_OUT}/modules/math_utils.slang-module"
            "${_SHADER_OUT}/modules/noise.slang-module"
    DEPFILE "${_SHADER_OUT}/compute_main.d"
    COMMENT "Slang→SPIR-V: compute_main"
)

# ── 预编译 SPIR-V（仅复制）────────────────────────────────────────
add_custom_command(
    OUTPUT "${_SHADER_OUT}/prebuilt_effect.spv"
    COMMAND ${CMAKE_COMMAND} -E copy "shaders/effect.spv"
            "${_SHADER_OUT}/prebuilt_effect.spv"
    DEPENDS "shaders/effect.spv"
)

# ── JIT 源码复制 ───────────────────────────────────────────────────
add_custom_command(
    OUTPUT "${_SHADER_OUT}/runtime_fx.slang"
    COMMAND ${CMAKE_COMMAND} -E copy "shaders/fx.slang"
            "${_SHADER_OUT}/runtime_fx.slang"
    DEPENDS "shaders/fx.slang" "${_SHADER_OUT}/modules/math_utils.slang-module"
)

# ── 整合目标（每次构建时确保所有着色器已编译）────────────────────
add_custom_target(game_shaders_compile
    DEPENDS
        "${_SHADER_OUT}/triangle_vert.spv"
        "${_SHADER_OUT}/triangle_frag.spv"
        "${_SHADER_OUT}/compute_main.spv"
        "${_SHADER_OUT}/prebuilt_effect.spv"
        "${_SHADER_OUT}/runtime_fx.slang"
)

# ── 嵌入模式：代码生成（调用 xxd 风格的生成器）───────────────────
add_custom_command(
    OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/shader-registry/game_shaders.hpp"
        "${CMAKE_CURRENT_BINARY_DIR}/shader-registry/game_shaders.cpp"
    COMMAND ${CMAKE_COMMAND}
        -D INPUT_DIR="${_SHADER_OUT}"
        -D OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/shader-registry"
        -D NAMESPACE="GameShaders"
        -P "${AKR_SHADER_CODEGEN_DIR}/embed.cmake"
    DEPENDS game_shaders_compile
    COMMENT "Generating embedded shader registry: game_shaders"
)

# ── 着色器集库目标（INTERFACE，供 link 使用）──────────────────────
add_library(game_shaders INTERFACE)
add_dependencies(game_shaders game_shaders_compile)

target_include_directories(game_shaders INTERFACE
    "${CMAKE_CURRENT_BINARY_DIR}/shader-registry"
)

# ── 元数据（供上层 CMake 自省）────────────────────────────────────
set_target_properties(game_shaders PROPERTIES
    AKR_SHADER_COUNT "5"
    AKR_SHADER_NAMES "triangle_vert;triangle_frag;compute_main;prebuilt_effect;runtime_fx"
    AKR_SHADER_OUTPUT_DIR "${_SHADER_OUT}"
    AKR_CONSUMPTION "embedded"
    AKR_EMBED_TARGET "my_app"
)

# 如果消费方已定义，自动添加依赖
if(TARGET my_app)
    add_dependencies(my_app game_shaders)
    message(STATUS "game_shaders: build dependency added to my_app")
endif()
```

### 3.2 生成的 C++ 嵌入头文件（`shader_registry.hpp`）

```cpp
// 自动生成 —— 由 shaders.config.cpp 驱动。请勿手动编辑。
#pragma once
#include <AkRender/Shaders/ShaderSet.hpp>
#include <cstdint>

namespace GameShaders {

    // ── 编译期索引枚举 ──────────────────────────────────────────
    // 顺序与 shaders.config.cpp 中 add_shader 的调用顺序一致
    enum ID : uint32_t {
        triangle_vert     = 0,
        triangle_frag     = 1,
        compute_main      = 2,
        prebuilt_effect   = 3,
        runtime_fx        = 4,
        COUNT             = 5,
    };

    // ── 编译期元数据（constexpr，零运行时开销）─────────────────
    constexpr AkRender::Shaders::ShaderInfo info[COUNT] = {
        { "triangle_vert",   AkRender::Shaders::SourceType::SpirV,
          AkRender::Shaders::Stage::Vertex,   "main" },
        { "triangle_frag",   AkRender::Shaders::SourceType::SpirV,
          AkRender::Shaders::Stage::Fragment, "main" },
        { "compute_main",    AkRender::Shaders::SourceType::SpirV,
          AkRender::Shaders::Stage::Compute,  "main" },
        { "prebuilt_effect", AkRender::Shaders::SourceType::SpirV,
          AkRender::Shaders::Stage::Compute,  "main" },
        { "runtime_fx",      AkRender::Shaders::SourceType::SlangSource,
          AkRender::Shaders::Stage::Compute,  "main" },
    };

    // ── 获取嵌入的着色器集 ──────────────────────────────────────
    const AkRender::Shaders::ShaderSet& get();
}
```

### 3.3 生成的 C++ 嵌入源文件（`shader_registry.cpp`）

```cpp
// 自动生成 —— 由 shaders.config.cpp 驱动。请勿手动编辑。
#include "game_shaders.hpp"

// ── 嵌入字节数组（alignas(16) 满足 Vulkan 要求）────────────────
namespace Detail {
    alignas(16) extern const unsigned char kTriangleVertSpv[];
    alignas(16) extern const unsigned char kTriangleFragSpv[];
    // ... 实际数据由 embed.cmake 从 .spv 文件中读取并生成
}
// 实际 .cpp 文件中这些数组会通过 #include "hex_data/triangle_vert.inc" 包含
// 或者由代码生成器直接写出十六进制字面量

static const AkRender::Shaders::ShaderSet s_instance(
    GameShaders::info,
    std::array<AkRender::Shaders::ShaderBlob, GameShaders::COUNT>{{
        { Detail::kTriangleVertSpv, sizeof(Detail::kTriangleVertSpv),
          GameShaders::info[GameShaders::triangle_vert] },
        { Detail::kTriangleFragSpv, sizeof(Detail::kTriangleFragSpv),
          GameShaders::info[GameShaders::triangle_frag] },
        // ...
    }}
);

const AkRender::Shaders::ShaderSet& GameShaders::get() { return s_instance; }
```

---

## 4. CMake 中的编排

### 4.1 主项目的使用方式

```cmake
# CMakeLists.txt（用户项目）

cmake_minimum_required(VERSION 3.21)
project(MyGame)

find_package(AkRender REQUIRED)

# ── 编译 C++ 配置工具 ─────────────────────────────────────────────
# 注：AkRender::ShaderConfigurator 是 AkRender 提供的静态库，
#     包含 Project、ShaderDecl 等类的实现
add_executable(shader_config_generator
    shaders.config.cpp
)
target_link_libraries(shader_config_generator PRIVATE
    AkRender::ShaderConfigurator
)

# ── 在配置阶段执行配置工具，生成 CMake 片段 ─────────────────────
# 这会产出 ${CMAKE_CURRENT_BINARY_DIR}/shader-gen/shaders.cmake
execute_process(
    COMMAND shader_config_generator
            "${CMAKE_CURRENT_BINARY_DIR}/shader-gen"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating shader configuration..."
)

# ── 引入生成的 CMake 片段 ─────────────────────────────────────────
# 其中定义了 game_shaders INTERFACE 库目标
include("${CMAKE_CURRENT_BINARY_DIR}/shader-gen/shaders.cmake")

# ── 构建主程序 ────────────────────────────────────────────────────
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE
    AkRender::AkRender
    game_shaders             # 来自生成的 CMake
)

# 嵌入模式不需要额外操作——game_shaders 已经包含了编译依赖
# 和生成的注册表源文件
```

### 4.2 构建阶段 vs 配置阶段

C++ 配置工具的执行时机有两种选择：

| 时机 | 方式 | 优缺点 |
|------|------|--------|
| **配置阶段** | `execute_process(COMMAND ...)` | 简单直接；但如果 shaders.config.cpp 本身变更，需要 re-configure |
| **构建阶段** | `add_custom_command(COMMAND ...)` + `include()` 生成的 cmake | 更干净——每次构建自动重新生成；但需要两次 configure |

**推荐方案**：配置阶段执行，因为：
- 着色器集的结构变更频率低
- 配置阶段生成后 CMake 能直接 `include()` 产物
- 着色器源文件的变更由生成的 `add_custom_command(DEPENDS ...)` 追踪，不影响配置工具本身

如需避免每次 re-configure，可在配置工具输出中写入一个时间戳文件，
CMake 侧的 `execute_process` 只在配置文件变更时才执行：

```cmake
# 如果 shaders.config.cpp 没变且生成产物已存在，跳过执行
set(_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/shader-gen")
set(_stamp "${_gen_dir}/.config_stamp")

if(NOT EXISTS "${_stamp}"
    OR "${CMAKE_CURRENT_SOURCE_DIR}/shaders.config.cpp" IS_NEWER_THAN "${_stamp}")
    # ... execute_process ...
    file(TOUCH "${_stamp}")
endif()
```

### 4.3 作为 AkRender 子项目使用

AkRender 自身也可以使用这套机制来管理内置着色器：

```cmake
# AkRender/cmake/Shaders/CMakeLists.txt（新增）
# AkRender 内部使用的着色器集

add_executable(akr_internal_shader_config
    akr_internal_shaders.cpp    # 用 C++ 配置 AkRender 的内置着色器
)
target_link_libraries(akr_internal_shader_config PRIVATE
    AkRender::ShaderConfigurator
)

execute_process(
    COMMAND akr_internal_shader_config
            "${CMAKE_CURRENT_BINARY_DIR}/shader-gen"
)

include("${CMAKE_CURRENT_BINARY_DIR}/shader-gen/shaders.cmake")
# 生成的 akr_core_shaders 目标被 AkRender 库自身链接
target_link_libraries(AkRender PRIVATE akr_core_shaders)
```

---

## 5. 工程结构

```
AkRender/
├── cmake/
│   ├── AkRenderConfig.cmake.in
│   ├── SlangShaderCompiler.cmake
│   └── codegen/
│       └── embed.cmake               # CMake 脚本：将 .spv → C++ 字节数组
│
├── tools/
│   ├── Shaders/                       # 已有的 Shaders 库（运行时 API）
│   │   ├── CMakeLists.txt
│   │   ├── include/AkRender/Shaders/
│   │   │   ├── ShaderSource.hpp       # 已有：基础类型
│   │   │   ├── ShaderSet.hpp          # 新增：运行时 ShaderSet 句柄
│   │   │   ├── ShaderRegistry.hpp     # 新增：全局注册表
│   │   │   └── SlangJIT.hpp           # 新增：JIT 编译器接口
│   │   └── src/
│   │       ├── ShaderManifest.cpp
│   │       ├── ShaderSet.cpp
│   │       ├── ShaderRegistry.cpp
│   │       └── SlangJIT.cpp
│   │
│   └── ShaderConfigurator/            # 新增：C++ 配置工具
│       ├── CMakeLists.txt
│       ├── include/AkRender/ShaderConfigurator/
│       │   ├── Project.hpp            # ShaderProject 类
│       │   ├── ShaderDecl.hpp         # ShaderDecl 类
│       │   ├── CodegenBackend.hpp     # 代码生成器接口
│       │   └── Backends/
│       │       ├── CMakeBackend.hpp   # → shaders.cmake 生成器
│       │       └── CppEmbedBackend.hpp# → .hpp/.cpp 嵌入代码生成器
│       └── src/
│           ├── Project.cpp
│           ├── ShaderDecl.cpp
│           ├── CodegenBackend.cpp
│           └── Backends/
│               ├── CMakeBackend.cpp
│               └── CppEmbedBackend.cpp
│
└── include/AkRender/Shaders/          # 安装后的公共头文件
    ├── ShaderSource.hpp
    ├── ShaderSet.hpp
    ├── ShaderRegistry.hpp
    └── SlangJIT.hpp
```

---

## 6. 代码生成器架构

### 6.1 后端插件式设计

```cpp
// tools/ShaderConfigurator/include/AkRender/ShaderConfigurator/CodegenBackend.hpp

namespace AkRender::ShaderConfigurator {

    class ShaderProject;  // forward

    // ── 代码生成器基类 ──────────────────────────────────────────
    // 每种输出格式实现一个后端
    class CodegenBackend {
    public:
        virtual ~CodegenBackend() = default;
        virtual std::string name() const = 0;

        // 生成所有文件到 output_dir
        virtual void generate(const ShaderProject& project,
                              const std::filesystem::path& output_dir) = 0;
    };

    // ── 内置后端 ────────────────────────────────────────────────
    // CMakeBackend: 生成 shaders.cmake（编译规则 + 库目标）
    // CppEmbedBackend: 生成 shader_registry.hpp + .cpp（嵌入数据）
    // CppDeployBackend: 生成 shader_paths.hpp（路径表）
    // JsonManifestBackend: 生成 shaders.json（供外部工具消费）
}
```

### 6.2 `ShaderProject::generate()` 的内部流程

```cpp
void ShaderProject::generate(const std::filesystem::path& output_dir) {
    // 1. 创建输出目录
    std::filesystem::create_directories(output_dir);

    // 2. 验证着色器声明的正确性
    validate();

    // 3. 解析模块依赖图（拓扑排序）
    resolve_dependencies();

    // 4. 根据消费策略决定后端组合
    std::vector<std::unique_ptr<CodegenBackend>> backends;

    // CMake 片段总是需要
    backends.push_back(std::make_unique<CMakeBackend>());

    switch (m_consumption) {
        case Consumption::Embedded:
            backends.push_back(std::make_unique<CppEmbedBackend>());
            break;
        case Consumption::Deployed:
            backends.push_back(std::make_unique<CppDeployBackend>());
            break;
    }

    // 5. 执行所有后端的生成逻辑
    for (auto& backend : backends) {
        backend->generate(*this, output_dir);
    }
}
```

---

## 7. 依赖管理（C++ 侧处理）

着色器的依赖关系在 C++ 配置工具中静态分析：

```cpp
// ShaderProject 内部
void ShaderProject::resolve_dependencies() {
    // 1. 按相同模块名归并 Slang 源文件
    //    add_module("math", {"a.slang", "b.slang"})
    //    → 一条 slangc -emit-ir 命令

    // 2. 对每个 Slang 着色器，追踪其 link_module 引用
    //    → 生成 add_custom_command 的 DEPENDS 列表

    // 3. 检测模块间的传递依赖
    //    如果 A import B，B import C，则 A 的 DEPENDS 包含 B 和 C

    // 4. 拓扑排序，确保模块在着色器之前编译

    // 5. 生成正确的 -I 路径列表
    //    传递给 slangc 的所有模块源目录 + 模块输出目录
}
```

这部分逻辑在 C++ 中写起来非常自然——标准容器、图算法、文件系统操作，
完全不需要 CMake 的宏/函数体操。

---

## 8. 与现有系统的兼容

### 8.1 向后兼容

已有的 `SlangShaderCompiler.cmake` 和示例可以继续工作。
新系统与其关系：

```
新的 C++ 配置工具
    │
    ├── 可以生成调用已有 SlangShaderCompiler.cmake 函数的 CMake 代码
    ├── 也可以完全独立，生成自包含的 add_custom_command
    └── 两种方式由 C++ 配置工具中的 CMakeBackend 决定
```

### 8.2 渐进式采用

```cmake
# 已有项目可以逐步迁移：
# 第 1 步：在现有 CMakeLists.txt 末尾引入生成的 cmake（不删除旧配置）
# 第 2 步：确认生成的构建规则正确后，逐步替换旧规则
# 第 3 步：完全迁移到 shaders.config.cpp
```

---

## 9. 关键技术决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 配置语言 | C++（非 Python/JSON/YAML） | 类型安全、IDE 支持、可测试、与现有工具链一致 |
| 执行时机 | 配置阶段 (`execute_process`) | 避免两次 configure，生成的 CMake 可直接 `include()` |
| 代码生成后端 | 插件式（可扩展） | 支持新的输出格式（JSON、WGSL、等）不影响核心逻辑 |
| 着色器声明顺序 | 决定枚举 ID 顺序 | 可预测、稳定（除非显式重排） |
| 增量构建 | CMake `add_custom_command(DEPENDS ...)` | 原生 Ninja depfile 支持 |
| 工具链发现 | 生成的 CMake 中 `find_program` | 用户可覆盖路径，失败时给出清晰提示 |

---

## 10. 路线图

| 阶段 | 内容 |
|------|------|
| **P0** | `ShaderConfigurator` 库骨架（Project + ShaderDecl + CMakeBackend） |
| **P1** | CMakeBackend：生成包含 GLSL/Slang/SPIR-V 编译规则的 `shaders.cmake` |
| **P2** | CppEmbedBackend：从 .spv 生成嵌入字节数组 + 索引枚举 |
| **P3** | `embed.cmake` CMake 脚本（将二进制文件转为 C++ 头文件的辅助工具） |
| **P4** | CppDeployBackend：部署模式的路径表生成 |
| **P5** | `ShaderSet` / `ShaderRegistry` C++ 运行时 API |
| **P6** | Slang JIT 支持（`SlangJITCompiler`） |
| **P7** | AkRender 自身内置着色器迁移到这套系统 |
| **P8** | HLSL 编译后端（DXC）+ 工具检测 |
| **P9** | 测试、文档、示例 |

---

## 11. 这个设计为什么好

1. **配置即代码**：C++ 是类型安全的 DSL，复杂依赖关系用图算法处理，不需要 CMake 的宏魔法
2. **IDE 支持**：写配置时有自动补全、跳转定义、重构支持
3. **可测试**：`ShaderProject` 的逻辑可单元测试，生成的 CMake 片段可用 `cmake -P` 验证
4. **无运行时开销**：枚举索引在编译期解析，`constexpr` 元数据在 `.rodata` 段
5. **增量构建**：`depfile` 追踪 Slang import 链，源文件变更只重编译受影响的着色器
6. **CMake 只做它擅长的事**：编排构建规则、管理目标——不做复杂逻辑
