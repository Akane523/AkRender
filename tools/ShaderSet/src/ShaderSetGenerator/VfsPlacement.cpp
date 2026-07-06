//===------ VfsPlacement.cpp ----------------------------------------------===//

#include <AkRender/ShaderSetGenerator/VfsPlacement.hpp>

#include <AkRender/ShaderSetGenerator/Manifest.hpp>

namespace fs = std::filesystem;

namespace AkRender::ShaderSetGenerator
{

VfsPlacement VfsPlacement::defaults(const Manifest &manifest)
{
  return {
      .source_root = manifest.source_root(),
      .vfs_prefix = manifest.vfs_prefix(),
  };
}

std::expected<Config::VirtualPath, std::string>
VfsPlacement::resolve(std::string_view manifest_name,
                      Config::SourcePath source) const
{
  if (!mapping_set)
  {
    return std::unexpected(
        "VfsPlacement mapping not selected (use map_parallel, map_basename, "
        "map_by_name, or map_exact)");
  }

  const fs::path batch_root =
      source_root.empty() ? fs::path(".") : source_root;

  return Config::resolve_vfs_path(mapping, manifest_name, source, batch_root,
                                  vfs_prefix);
}

VfsPlacement with_source_root(VfsPlacement placement, fs::path root)
{
  placement.source_root = std::move(root);
  return placement;
}

VfsPlacement with_vfs_prefix(VfsPlacement placement, Config::VirtualPath prefix)
{
  placement.vfs_prefix = std::move(prefix);
  return placement;
}

VfsPlacement map_parallel(VfsPlacement placement)
{
  placement.mapping = Config::VfsMapping{.kind = Config::VfsMapping::Kind::Parallel};
  placement.mapping_set = true;
  return placement;
}

VfsPlacement map_basename(VfsPlacement placement)
{
  placement.mapping = Config::VfsMapping{.kind = Config::VfsMapping::Kind::Basename};
  placement.mapping_set = true;
  return placement;
}

VfsPlacement map_by_name(VfsPlacement placement)
{
  placement.mapping = Config::VfsMapping{.kind = Config::VfsMapping::Kind::ByName};
  placement.mapping_set = true;
  return placement;
}

VfsPlacement map_exact(VfsPlacement placement, Config::VirtualPath path)
{
  placement.mapping = Config::VfsMapping{
      .kind = Config::VfsMapping::Kind::Exact,
      .exact_path = std::move(path),
  };
  placement.mapping_set = true;
  return placement;
}

} // namespace AkRender::ShaderSetGenerator
