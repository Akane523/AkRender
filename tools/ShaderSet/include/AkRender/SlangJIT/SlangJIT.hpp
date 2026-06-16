#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace AkRender::ShaderSet
{

// ═════════════════════════════════════════════════════════════════════════════
//  Shader stage
// ═════════════════════════════════════════════════════════════════════════════

/// @brief Shader pipeline stage.
enum class Stage : uint8_t
{
  Vertex,
  Fragment,
  Compute,
  Geometry,
  TessControl,
  TessEval,
  Mesh,
  Task,
  RayGeneration,
  RayIntersection,
  RayAnyHit,
  RayClosestHit,
  RayMiss,
  RayCallable,
  Amplification,
};

// ═════════════════════════════════════════════════════════════════════════════
//  Compile target format
// ═════════════════════════════════════════════════════════════════════════════

/// @brief Downstream target format for Slang compilation.
enum class TargetFormat : uint8_t
{
  SpirV, // SPIR-V (Vulkan)
  DXIL,  // DirectX Shader Model 6.x
  WGSL,  // WebGPU
};

// ═════════════════════════════════════════════════════════════════════════════
//  Compile option helpers
// ═════════════════════════════════════════════════════════════════════════════

/// @brief Floating-point mode for Slang compilation.
enum class FloatMode : uint8_t
{
  Default,
  Precise,
  Fast,
};

/// @brief Matrix layout for SPIR-V generation.
enum class MatrixLayout : uint8_t
{
  ColumnMajor,
  RowMajor,
};

/// @brief Optimization level for Slang compilation.
enum class OptimizationLevel : uint8_t
{
  O0,
  O1,
  O2,
  O3,
};

/// @brief Preprocessor definition for Slang compilation.
struct PreprocessorDefine
{
  std::string_view name;
  std::string_view value;
};

/// @brief Aggregate compile options passed to slangc or the Slang JIT API.
struct CompileOptions
{
  TargetFormat target_format = TargetFormat::SpirV;
  OptimizationLevel optimization = OptimizationLevel::O2;
  FloatMode float_mode = FloatMode::Default;
  MatrixLayout matrix_layout = MatrixLayout::RowMajor;
  bool debug_info = false;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Compile result
// ═════════════════════════════════════════════════════════════════════════════

/// @brief Result of a JIT compilation.
struct CompileResult
{
  /// Compiled binary (e.g. SPIR-V words).
  std::vector<uint32_t> binary;

  /// Human-readable diagnostic messages (errors / warnings).
  std::string diagnostic;

  /// True if compilation succeeded with no errors.
  bool success = false;
};

// ═════════════════════════════════════════════════════════════════════════════
//  SlangJITCompiler — runtime Slang compilation
// ═════════════════════════════════════════════════════════════════════════════
//
// Wraps the Slang C++ API to provide runtime JIT compilation of Slang shaders.
//
// Typical usage:
//   1. Create a SlangJITCompiler instance.
//   2. createSession() — initialise a Slang compilation session with desired
//      search paths and compile options.
//   3. loadModuleFromIR() — load pre-compiled .slang-module IR blobs.
//   4. compileEntryPoint() — compose loaded modules and generate SPIR-V.
//
// Lifetime:
//   The compiler must outlive any compiled results that reference Slang
//   internal data.  Modules loaded into a session are released when the
//   session is destroyed.
//
// Thread safety:
//   A SlangJITCompiler instance is NOT thread-safe.  Use one instance per
//   thread, or externally synchronise access.

class SlangJITCompiler
{
public:
  SlangJITCompiler();
  ~SlangJITCompiler();

  // Non-copyable, non-movable (owns Slang API objects).
  SlangJITCompiler(const SlangJITCompiler &) = delete;
  SlangJITCompiler &operator=(const SlangJITCompiler &) = delete;
  SlangJITCompiler(SlangJITCompiler &&) = delete;
  SlangJITCompiler &operator=(SlangJITCompiler &&) = delete;

  // ── Session management ──────────────────────────────────────────────────

  /// Create a Slang compilation session.
  ///
  /// @param searchPaths  Directories to search for #include / import.
  /// @param defines      Preprocessor macros for the session.
  /// @param options      Compile options (target format, optimisation, etc.).
  /// @return true on success.
  bool createSession(std::span<const std::filesystem::path> searchPaths = {},
                     std::span<const PreprocessorDefine> defines = {},
                     const CompileOptions &options = {});

  /// Destroy the current session and all loaded modules.
  void destroySession();

  // ── Module loading ──────────────────────────────────────────────────────

  /// Load a pre-compiled .slang-module IR blob into the session.
  ///
  /// The IR blob is typically embedded in the application binary and produced
  /// at build time by `slangc -emit-ir -module-name <name>`.
  ///
  /// @param moduleName  Name used for `import` resolution.
  /// @param data        Pointer to the IR binary.
  /// @param size        Size of the IR binary in bytes.
  /// @return true if the module was loaded successfully.
  bool loadModuleFromIR(std::string_view moduleName, const void *data,
                        size_t size);

  /// Load a Slang source module into the session.
  ///
  /// @param moduleName  Name used for `import` resolution.
  /// @param source      Slang source code.
  /// @param filePath    Virtual file path for diagnostics (e.g.
  /// "shader.slang").
  /// @return true if the module was loaded successfully.
  bool loadModuleFromSource(std::string_view moduleName,
                            std::string_view source,
                            std::string_view filePath = "source.slang");

  // ── Compilation ─────────────────────────────────────────────────────────

  /// Compose loaded modules and a named entry-point into target code.
  ///
  /// Links all currently loaded modules together, looks up the named
  /// entry-point (e.g. "main"), and generates target code (SPIR-V by default).
  ///
  /// @param entryPointName  Name of the entry-point function.
  /// @return CompileResult with the generated binary and diagnostics.
  CompileResult compileEntryPoint(std::string_view entryPointName);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace AkRender::ShaderSet
