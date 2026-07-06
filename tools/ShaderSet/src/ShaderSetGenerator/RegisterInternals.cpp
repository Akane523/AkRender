//===------ RegisterInternals.cpp -----------------------------------------===//

#include <AkRender/ShaderSetGenerator/RegisterInternals.hpp>

#include <AkRender/ShaderSetGenerator/VirtualPath.hpp>

#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace fs = std::filesystem;

namespace AkRender::ShaderSetGenerator::detail
{

namespace
{

fs::path compose_batch_root(const Manifest &manifest,
                            const fs::path &placement_source_root)
{
  fs::path batch_root =
      placement_source_root.empty() ? fs::path(".") : placement_source_root;
  if (const fs::path manifest_root = manifest.source_root();
      !manifest_root.empty())
  {
    batch_root = manifest_root / batch_root;
  }
  return batch_root;
}

Config::VirtualPath prefixed_default(std::string_view prefix,
                                     std::string_view suffix_path)
{
  if (prefix.empty())
    return require_normalized(suffix_path);

  const auto joined =
      Config::vfs_join(Config::VirtualPath{std::string(prefix)}, suffix_path);
  if (!joined)
    throw std::invalid_argument(joined.error());
  return *joined;
}

std::string replace_extension(std::string path, std::string_view new_ext)
{
  const std::size_t slash = path.rfind('/');
  const std::size_t dot = path.rfind('.');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
    path.resize(dot);
  path.append(new_ext);
  return path;
}

Config::VirtualPath resolve_mapped_or_default(
    const VfsPlacement &placement, std::string_view manifest_name,
    Config::SourcePath source, std::string_view default_suffix_path,
    std::string_view mapped_extension)
{
  if (placement.mapping_set)
  {
    const auto resolved = placement.resolve(manifest_name, source);
    if (!resolved)
      throw std::invalid_argument(resolved.error());
    return require_normalized(
        replace_extension(resolved->value, mapped_extension));
  }

  return prefixed_default(placement.vfs_prefix.value, default_suffix_path);
}

} // namespace

Config::SourcePath store_source_path(const Manifest &manifest,
                                     const VfsPlacement &placement,
                                     Config::SourcePath source)
{
  fs::path stored = source.path;
  if (!stored.is_absolute())
    stored = compose_batch_root(manifest, placement.source_root) / stored;
  return Config::SourcePath{std::move(stored)};
}

Config::VirtualPath require_normalized(std::string_view path)
{
  const auto normalized = Config::normalize_vfs_path(path);
  if (!normalized)
    throw std::invalid_argument(normalized.error());
  return *normalized;
}

Config::VirtualPath module_ir_path(const VfsPlacement &placement,
                                   std::string_view import_identity,
                                   std::string_view manifest_name,
                                   Config::SourcePath source)
{
  const std::string fallback =
      "/shaders/slang/" + std::string(import_identity) + ".slang-module";
  return resolve_mapped_or_default(placement, manifest_name, source, fallback,
                                   ".slang-module");
}

Config::VirtualPath shader_ir_path(const VfsPlacement &placement,
                                   std::string_view shader_name,
                                   Config::SourcePath source)
{
  const std::string fallback =
      "/shaders/slang/" + std::string(shader_name) + ".slang-module";
  return resolve_mapped_or_default(placement, shader_name, source, fallback,
                                   ".slang-module");
}

Config::VirtualPath shader_spv_path(const VfsPlacement &placement,
                                    std::string_view shader_name,
                                    Config::SourcePath source)
{
  const std::string fallback =
      "/shaders/spv/" + std::string(shader_name) + ".spv";
  return resolve_mapped_or_default(placement, shader_name, source, fallback,
                                   ".spv");
}

void assign_unique_dependencies(Config::SlangShader *shader,
                                const std::vector<std::string> &dependencies)
{
  shader->dependencies.clear();
  shader->dependencies.reserve(dependencies.size());
  std::unordered_set<std::string> seen;
  for (const std::string &dep : dependencies)
  {
    if (dep.empty())
      throw std::invalid_argument("shader dependency name is empty");
    if (!seen.insert(dep).second)
      continue;
    shader->dependencies.push_back(dep);
  }
}

void ensure_module_dependency(const Manifest &manifest,
                              std::string_view module_name)
{
  if (!manifest.find_slang_module(module_name))
  {
    throw std::invalid_argument("shader uses unknown slang module '"
                                + std::string(module_name) + "'");
  }
}

} // namespace AkRender::ShaderSetGenerator::detail
