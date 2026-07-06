#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <AkRender/ShaderSet/VirtualFileSystem.hpp>
#include <AkRender/SlangJIT/SlangJIT.hpp>

namespace AkRender::ShaderSet
{

class SlangJITCompiler;

// ═════════════════════════════════════════════════════════════════════════════
//  Embedded resource descriptors (generated as named constexpr variables)
// ═════════════════════════════════════════════════════════════════════════════

/// Manifest-registered binary resource embedded in the shader-set blob.
struct BinaryResourceDesc
{
  std::string_view manifest_name;
  std::string_view vfs_path;
  Record data;
};

/// Pre-compiled Slang module IR (.slang-module) embedded in the blob.
struct SlangModuleDesc
{
  /// Manifest registration name (lookup / generated variable name).
  std::string_view manifest_name;
  /// Slang \c import identity.
  std::string_view import_name;
  std::string_view vfs_path;
  Record ir;
};

/// Slang shader entry point compiled to IR at build time (JIT at runtime).
struct SlangShaderDesc
{
  std::string_view manifest_name;
  std::string_view vfs_path;
  Record ir;
  /// Offline SPIR-V when mode is SpirV or Both; empty Record otherwise.
  Record spirv;
  std::string_view entry_point;
  Stage stage;
  const SlangModuleDesc *module_deps;
  uint8_t num_module_deps;
  CompileOptions options;

  constexpr bool has_offline_spirv() const noexcept { return !spirv.empty(); }
};

/// Pre-compiled SPIR-V shader embedded in the blob.
struct SpirVShaderDesc
{
  std::string_view manifest_name;
  std::string_view vfs_path;
  Record spirv;
  std::string_view entry_point;
  Stage stage;
};

// ═════════════════════════════════════════════════════════════════════════════
//  Runtime helpers
// ═════════════════════════════════════════════════════════════════════════════

[[nodiscard]] inline std::span<const std::byte>
recordBytes(const Record &record, const void *blobData) noexcept
{
  if (!blobData || record.empty())
    return {};
  const auto *base = static_cast<const std::byte *>(blobData);
  return {base + record.offset, record.size};
}

[[nodiscard]] inline std::span<const std::byte>
moduleIRBytes(const SlangModuleDesc &module, const void *blobData) noexcept
{
  return recordBytes(module.ir, blobData);
}

[[nodiscard]] inline std::span<const std::byte>
spirvBytes(const SpirVShaderDesc &shader, const void *blobData) noexcept
{
  return recordBytes(shader.spirv, blobData);
}

bool loadSlangModule(SlangJITCompiler &compiler, const SlangModuleDesc &module,
                     const void *blobData);

CompileResult compileSlangShader(SlangJITCompiler &compiler,
                                 const SlangShaderDesc &shader,
                                 const void *blobData);

} // namespace AkRender::ShaderSet
