#pragma once

#include <cstdint>
#include <string_view>

namespace AkRender::Shaders
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
  SpirV,   // SPIR-V (Vulkan)
  DXIL,    // DirectX Shader Model 6.x
  WGSL,    // WebGPU
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
  TargetFormat      target_format = TargetFormat::SpirV;
  OptimizationLevel optimization  = OptimizationLevel::O2;
  FloatMode         float_mode    = FloatMode::Default;
  MatrixLayout      matrix_layout = MatrixLayout::RowMajor;
  bool              debug_info    = false;
};

} // namespace AkRender::Shaders
