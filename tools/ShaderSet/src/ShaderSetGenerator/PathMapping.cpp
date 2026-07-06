//===------ PathMapping.cpp -----------------------------------------------===//

#include <AkRender/ShaderSetGenerator/PathMapping.hpp>

#include <system_error>

namespace fs = std::filesystem;

namespace AkRender::ShaderSetGenerator::Config
{

namespace
{

std::expected<std::string, std::string>
relative_path_string(const fs::path &path, const fs::path &root)
{
  std::error_code ec;
  const fs::path rel = fs::relative(path, root, ec);
  if (ec)
  {
    return std::unexpected("cannot compute relative path from '" +
                           root.generic_string() + "' to '" +
                           path.generic_string() + "'");
  }

  auto rel_str = rel.generic_string();
  if (rel_str.empty() || rel_str == ".")
    return std::unexpected("source path equals source root");

  if (rel_str.starts_with(".."))
    return std::unexpected("source path escapes source root");

  return rel_str;
}

} // namespace

std::expected<fs::path, std::string>
resolve_source_path(const SourcePath &source, const fs::path &source_root)
{
  if (source.path.empty())
    return std::unexpected("source path is empty");

  if (source.path.is_absolute())
    return source.path;

  if (source_root.empty())
    return std::unexpected("relative source path requires a source root");

  return source_root / source.path;
}

std::expected<VirtualPath, std::string>
resolve_vfs_path(const VfsMapping &mapping, std::string_view resource_name,
                 const SourcePath &source, const fs::path &batch_source_root,
                 const VirtualPath &vfs_prefix)
{
  switch (mapping.kind)
  {
  case VfsMapping::Kind::Exact:
    if (mapping.exact_path.empty())
      return std::unexpected("exact VFS mapping requires a path");
    return mapping.exact_path;

  case VfsMapping::Kind::Parallel:
  {
    if (vfs_prefix.empty())
      return std::unexpected("parallel VFS mapping requires a vfs prefix");

    const auto abs_source = resolve_source_path(source, batch_source_root);
    if (!abs_source)
      return std::unexpected(abs_source.error());

    const auto rel = relative_path_string(*abs_source, batch_source_root);
    if (!rel)
      return std::unexpected(rel.error());

    return vfs_join(vfs_prefix, *rel);
  }

  case VfsMapping::Kind::Basename:
  {
    if (vfs_prefix.empty())
      return std::unexpected("basename VFS mapping requires a vfs prefix");

    const auto leaf = source.path.filename().generic_string();
    if (leaf.empty())
      return std::unexpected("source path has no filename");

    return vfs_join(vfs_prefix, leaf);
  }

  case VfsMapping::Kind::ByName:
  {
    if (vfs_prefix.empty())
      return std::unexpected("by-name VFS mapping requires a vfs prefix");

    if (resource_name.empty())
      return std::unexpected("resource name is empty");

    return vfs_join(vfs_prefix, resource_name);
  }
  }

  return std::unexpected("unknown VFS mapping kind");
}

} // namespace AkRender::ShaderSetGenerator::Config
