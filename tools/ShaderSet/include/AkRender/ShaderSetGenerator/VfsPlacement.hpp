//===------ VfsPlacement.hpp ----------------------------------------------===//
//
// Parameters for resolving manifest resource paths in the virtual file system.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/PathMapping.hpp>

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace AkRender::ShaderSetGenerator
{

class Manifest;

/// Layout inputs shared by disk embeds and shader compile registrations.
struct VfsPlacement
{
  std::filesystem::path source_root;
  Config::VirtualPath vfs_prefix;
  Config::VfsMapping mapping{};
  bool mapping_set = false;

  [[nodiscard]] static VfsPlacement defaults(const Manifest &manifest);

  [[nodiscard]] std::expected<Config::VirtualPath, std::string>
  resolve(std::string_view manifest_name, Config::SourcePath source) const;
};

[[nodiscard]] VfsPlacement with_source_root(VfsPlacement placement,
                                            std::filesystem::path root);

[[nodiscard]] VfsPlacement with_vfs_prefix(VfsPlacement placement,
                                           Config::VirtualPath prefix);

[[nodiscard]] VfsPlacement map_parallel(VfsPlacement placement);

[[nodiscard]] VfsPlacement map_basename(VfsPlacement placement);

[[nodiscard]] VfsPlacement map_by_name(VfsPlacement placement);

[[nodiscard]] VfsPlacement map_exact(VfsPlacement placement,
                                     Config::VirtualPath path);

template <typename Fn>
  requires requires(Fn fn, VfsPlacement placement) {
    { fn(placement) } -> std::same_as<VfsPlacement>;
  }
[[nodiscard]] VfsPlacement operator|(VfsPlacement placement, Fn fn)
{
  return fn(placement);
}

} // namespace AkRender::ShaderSetGenerator
