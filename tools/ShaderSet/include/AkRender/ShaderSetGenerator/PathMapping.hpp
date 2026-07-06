//===------ PathMapping.hpp -----------------------------------------------===//
//
// Source / VFS path resolution for binary resource embedding.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/VirtualPath.hpp>

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace AkRender::ShaderSetGenerator::Config
{

/// Physical source file location (absolute or relative to a source root).
struct SourcePath
{
  std::filesystem::path path{};

  SourcePath() = default;
  SourcePath(std::filesystem::path p) : path(std::move(p)) {}
  SourcePath(const char *p) : path(p) {}

  friend bool operator==(const SourcePath &, const SourcePath &) = default;
};

/// Explicit rule for computing a resource VFS path at registration time.
struct VfsMapping
{
  enum class Kind : std::uint8_t
  {
    Exact,
    Parallel,
    Basename,
    ByName,
  };

  Kind kind = Kind::Exact;
  VirtualPath exact_path{};
};

/// Resolve a physical source path against \p source_root.
std::expected<std::filesystem::path, std::string>
resolve_source_path(const SourcePath &source,
                    const std::filesystem::path &source_root);

/// Compute the absolute VFS path for a resource using \p mapping.
std::expected<VirtualPath, std::string>
resolve_vfs_path(const VfsMapping &mapping,
                 std::string_view resource_name,
                 const SourcePath &source,
                 const std::filesystem::path &batch_source_root,
                 const VirtualPath &vfs_prefix);

} // namespace AkRender::ShaderSetGenerator::Config
