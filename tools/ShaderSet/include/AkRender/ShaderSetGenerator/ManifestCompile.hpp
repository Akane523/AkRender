//===------ ManifestCompile.hpp -------------------------------------------===//
//
// Build-time Slang compilation orchestration for ShaderSetGenerator.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/Manifest.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <inja/inja.hpp>

namespace AkRender::ShaderSetGenerator
{

enum class BlobSegmentKind : std::uint8_t
{
  Binary,
  ModuleIR,
  ShaderIR,
  ShaderSpirV,
};

/// One contiguous region in the embedded blob.
struct BlobSegment
{
  BlobSegmentKind kind = BlobSegmentKind::Binary;
  /// Manifest registration name (user-facing identity).
  std::string manifest_name;
  std::string vfs_path;
  std::vector<std::byte> data;
  std::size_t blob_offset = 0;
};

/// Metadata for inja template generation of shader descriptors.
struct ShaderCodegenData
{
  std::vector<BlobSegment> segments;
  std::vector<inja::json> slang_modules;
  std::vector<inja::json> slang_shaders;
  std::vector<inja::json> spirv_shaders;
  std::vector<std::filesystem::path> source_dependencies;
};

ShaderCodegenData compile_manifest_shaders(
    const Manifest &manifest, const std::filesystem::path &source_dir,
    bool verbose = false);

} // namespace AkRender::ShaderSetGenerator
