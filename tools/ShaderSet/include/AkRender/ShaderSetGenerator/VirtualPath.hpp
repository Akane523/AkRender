//===------ VirtualPath.hpp -----------------------------------------------===//
//
// Normalized absolute paths for the ShaderSet virtual file system.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace AkRender::ShaderSetGenerator::Config
{

/// Absolute VFS path using POSIX '/' separators (always starts with '/').
struct VirtualPath
{
  std::string value;

  constexpr bool empty() const noexcept { return value.empty(); }

  /// Explicit root-level file: "/" + \p name (single path component).
  static VirtualPath from_name(std::string_view name);

  friend bool operator==(const VirtualPath &, const VirtualPath &) = default;
};

/// Normalize \p raw into an absolute VFS path.
///
/// Rules:
///   - Result starts with '/'
///   - Collapses repeated '/'
///   - Rejects '\\', '..', empty segments, trailing '/'
std::expected<VirtualPath, std::string>
normalize_vfs_path(std::string_view raw);

/// Join \p base and \p leaf.  \p leaf must be relative (no leading '/').
/// Both arguments are normalized; the result is a file path (no trailing '/').
std::expected<VirtualPath, std::string>
vfs_join(const VirtualPath &base, std::string_view leaf);

} // namespace AkRender::ShaderSetGenerator::Config
