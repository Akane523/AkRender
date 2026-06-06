#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <AkRender/ShaderSet/SlangJIT.hpp>
#include <AkRender/ShaderSet/VirtualFileSystem.hpp>

namespace AkRender::Shaders
{

// ═════════════════════════════════════════════════════════════════════════════
//  Forward declarations
// ═════════════════════════════════════════════════════════════════════════════

struct SlangModule;

// ═════════════════════════════════════════════════════════════════════════════
//  SlangModule — pre-compiled module descriptor
// ═════════════════════════════════════════════════════════════════════════════
//
// Slang modular compilation model:
//
//   Build-time (offline):
//     slangc math.slang -emit-ir -module-name math -o math.slang-module
//       -emit-ir      → emit Slang IR (skip target codegen)
//       -module-name  → set module identity for `import` resolution
//       output        → .slang-module (serialized IRModule binary)
//
//   Runtime (JIT):
//     session->loadModuleFromIRBlob("math_utils", data, size, ...)
//       loads pre-compiled IR into the Slang session
//       so that shaders doing `import math_utils` can find it
//
// Why pre-compiled .slang-module IR instead of raw .slang source?
//   1. Faster startup — loadModuleFromIRBlob skips lexing/parsing/type
//      checking and directly deserializes the IR.
//   2. Smaller binary — IR is more compact than source (stripped
//      whitespace, comments; identifiers are interned).
//   3. Consistency — the compiler version used at build time is pinned,
//      avoiding behavioral drift at runtime.
//
// Each SlangModule corresponds to one .slang-module (IR container).
// Its binary content is referenced via a Record into the embedded blob.
//
/// @brief Describes a pre-compiled Slang module (.slang-module IR container).
struct SlangModule
{
  /// Module name used in `import` (e.g. "math_utils").
  std::string_view name;

  /// Record pointing to the .slang-module IR binary in the embedded blob.
  Record data;
};

// ═════════════════════════════════════════════════════════════════════════════
//  SlangShader — JIT compilation descriptor
// ═════════════════════════════════════════════════════════════════════════════
//
// Everything — modules AND entry-point shaders — is pre-compiled to
// .slang-module IR at build time via `slangc -emit-ir`.  The runtime never
// touches raw .slang source; it only calls loadModuleFromIRBlob().
//
// Runtime JIT flow (using the Slang API):
//   1. Load dependent modules:  session->loadModuleFromIRBlob(mod.name, data, ...)
//   2. Load shader IR:          session->loadModuleFromIRBlob("shader", data, ...)
//   3. Find entry-point:        module->findEntryPointByName("main")
//   4. Compose + link:          session->createCompositeComponentType(...)
//   5. Generate target code:    composite->getEntryPointCode(0, 0, &spirvBlob)
//
// The entry-point function name and compile options are the only pieces of
// data that are not embedded in the IR itself and need explicit metadata.
//
/// @brief Describes a shader entry-point compiled from Slang at runtime.
struct SlangShader
{
  /// Record pointing to the .slang-module IR binary in the embedded blob.
  Record ir;

  /// Entry-point function name (e.g. "main", "vsMain").
  std::string_view entry_point;

  /// Pipeline stage this shader compiles to.
  Stage stage;

  /// Pointer to the first dependent SlangModule, or nullptr if none.
  const SlangModule* module_deps;

  /// Number of entries in module_deps.
  uint8_t num_module_deps;

  /// Compile options for the Slang JIT invocation.
  CompileOptions options;
};

// ═════════════════════════════════════════════════════════════════════════════
//  SpirV_Shader — pre-compiled SPIR-V descriptor
// ═════════════════════════════════════════════════════════════════════════════

/// @brief Describes a shader pre-compiled to SPIR-V at build time.
///
/// The SPIR-V binary is stored in the embedded blob and referenced via Record.
/// At runtime it is passed directly to vkCreateShaderModule.
///
struct SpirV_Shader
{
  /// Record pointing to the .spv binary data in the embedded blob.
  Record spirv;

  /// Entry-point function name (e.g. "main").
  std::string_view entry_point;

  /// Pipeline stage this shader targets.
  Stage stage;
};

} // namespace AkRender::Shaders