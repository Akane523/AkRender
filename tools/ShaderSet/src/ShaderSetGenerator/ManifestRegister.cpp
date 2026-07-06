//===------ ManifestRegister.cpp ------------------------------------------===//

#include <AkRender/ShaderSetGenerator/ManifestRegister.hpp>

#include <AkRender/ShaderSetGenerator/ModuleBuilder.hpp>
#include <AkRender/ShaderSetGenerator/RegisterInternals.hpp>
#include <AkRender/ShaderSetGenerator/SlangBuilder.hpp>
#include <AkRender/ShaderSetGenerator/SpirVFileBuilder.hpp>
#include <AkRender/ShaderSetGenerator/VirtualPath.hpp>

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace AkRender::ShaderSetGenerator
{

ScopedVfsPrefix::ScopedVfsPrefix(Manifest &manifest, Config::VirtualPath prefix)
    : m_manifest(&manifest), m_previous(manifest.vfs_prefix())
{
  m_manifest->set_vfs_prefix(std::move(prefix));
}

ScopedVfsPrefix::~ScopedVfsPrefix()
{
  if (m_manifest)
    m_manifest->set_vfs_prefix(std::move(m_previous));
}

ScopedSourceRoot::ScopedSourceRoot(Manifest &manifest, fs::path root)
    : m_manifest(&manifest), m_previous(manifest.source_root())
{
  m_manifest->set_source_root(std::move(root));
}

ScopedSourceRoot::~ScopedSourceRoot()
{
  if (m_manifest)
    m_manifest->set_source_root(std::move(m_previous));
}

ManifestRegister open(Manifest &manifest)
{
  return {
      .manifest = manifest,
      .placement = VfsPlacement::defaults(manifest),
  };
}

ManifestRegister from(ManifestRegister reg, fs::path source_root)
{
  reg.placement = with_source_root(std::move(reg.placement), std::move(source_root));
  return reg;
}

ManifestRegister into(ManifestRegister reg, Config::VirtualPath vfs_prefix)
{
  reg.placement = with_vfs_prefix(std::move(reg.placement), std::move(vfs_prefix));
  return reg;
}

ManifestRegister file(ManifestRegister reg, std::string name,
                      Config::SourcePath source)
{
  const auto vfs = reg.placement.resolve(name, source);
  if (!vfs)
    throw std::invalid_argument(vfs.error());

  auto *resource = reg.manifest.add_binary_resource(std::move(name));
  resource->source_path =
      detail::store_source_path(reg.manifest, reg.placement, source);
  resource->vfs_path = *vfs;
  return reg;
}

ManifestRegister files(
    ManifestRegister reg,
    std::initializer_list<std::pair<std::string, Config::SourcePath>> entries)
{
  for (const auto &[name, source] : entries)
    (void)file(reg, name, source);
  return reg;
}

ManifestRegister file_at(ManifestRegister reg, std::string name,
                         Config::SourcePath source,
                         Config::VirtualPath absolute_vfs)
{
  auto *resource = reg.manifest.add_binary_resource(std::move(name));
  resource->source_path = std::move(source);
  resource->vfs_path = detail::require_normalized(absolute_vfs.value);
  return reg;
}

ManifestRegister tree(ManifestRegister reg, fs::path source_dir,
                      Config::VirtualPath vfs_prefix, TreeNamePolicy name_policy)
{
  if (source_dir.empty())
    throw std::invalid_argument("tree source_dir is empty");
  if (vfs_prefix.empty())
    throw std::invalid_argument("tree requires a vfs prefix");

  fs::path tree_root = source_dir;
  if (!tree_root.is_absolute() && !reg.manifest.source_root().empty())
    tree_root = reg.manifest.source_root() / tree_root;

  std::error_code ec;
  if (!fs::exists(tree_root, ec) || !fs::is_directory(tree_root, ec))
  {
    throw std::invalid_argument("tree source_dir is not a directory: " +
                                tree_root.generic_string());
  }

  reg.placement = map_parallel(std::move(reg.placement));
  reg.placement = with_vfs_prefix(std::move(reg.placement), std::move(vfs_prefix));
  reg.placement = with_source_root(std::move(reg.placement), source_dir);

  std::unordered_set<std::string> seen_names;
  for (const fs::directory_entry &entry :
       fs::recursive_directory_iterator(tree_root, ec))
  {
    if (ec)
    {
      throw std::invalid_argument("tree failed to iterate '" +
                                  tree_root.generic_string() + "': " +
                                  ec.message());
    }

    if (!entry.is_regular_file(ec))
      continue;

    const fs::path rel = fs::relative(entry.path(), tree_root, ec);
    if (ec || rel.empty())
      continue;

    std::string name;
    switch (name_policy)
    {
    case TreeNamePolicy::RelativePath:
      name = rel.generic_string();
      std::replace(name.begin(), name.end(), '/', '_');
      break;
    case TreeNamePolicy::Stem:
      name = rel.stem().generic_string();
      break;
    }

    if (name.empty())
      throw std::invalid_argument("tree produced an empty resource name");

    if (!seen_names.insert(name).second)
    {
      throw std::invalid_argument("tree duplicate resource name '" + name + "'");
    }

    (void)file(reg, name, Config::SourcePath{rel});
  }

  return reg;
}

ManifestRegister module(ManifestRegister reg, std::string name,
                        std::initializer_list<fs::path> sources,
                        std::string module_name)
{
  ModuleBuilder builder{std::move(reg), std::move(name)};
  builder.sources(sources);
  if (!module_name.empty())
    builder.import_as(std::move(module_name));
  return commit_module_builder(std::move(builder));
}

ManifestRegister ir(ManifestRegister reg, std::string name,
                    Config::SourcePath source, std::string_view entry_point,
                    AkRender::ShaderSet::Stage stage,
                    std::initializer_list<std::string_view> dependencies)
{
  SlangBuilder builder{std::move(reg), std::move(name)};
  builder.source(source.path).entry(entry_point).stage(stage).ir();
  for (std::string_view dep : dependencies)
    builder.uses(dep);
  return commit_slang_builder(std::move(builder));
}

ManifestRegister spirv(ManifestRegister reg, std::string name,
                       Config::SourcePath source, std::string_view entry_point,
                       AkRender::ShaderSet::Stage stage,
                       std::initializer_list<std::string_view> dependencies)
{
  SlangBuilder builder{std::move(reg), std::move(name)};
  builder.source(source.path).entry(entry_point).stage(stage).spirv();
  for (std::string_view dep : dependencies)
    builder.uses(dep);
  return commit_slang_builder(std::move(builder));
}

ManifestRegister both(ManifestRegister reg, std::string name,
                      Config::SourcePath source, std::string_view entry_point,
                      AkRender::ShaderSet::Stage stage,
                      std::initializer_list<std::string_view> dependencies)
{
  SlangBuilder builder{std::move(reg), std::move(name)};
  builder.source(source.path).entry(entry_point).stage(stage).both();
  for (std::string_view dep : dependencies)
    builder.uses(dep);
  return commit_slang_builder(std::move(builder));
}

ManifestRegister spirv_file(ManifestRegister reg, std::string name,
                            Config::SourcePath source,
                            std::string_view entry_point,
                            AkRender::ShaderSet::Stage stage)
{
  SpirVFileBuilder builder{std::move(reg), std::move(name)};
  builder.source(source.path).entry(entry_point).stage(stage);
  return commit_spirv_file_builder(std::move(builder));
}

void embed_parallel(
    Manifest &manifest, Config::VirtualPath vfs_prefix, fs::path source_root,
    std::initializer_list<std::pair<std::string, Config::SourcePath>> entries)
{
  open(manifest) | register_all(into(std::move(vfs_prefix)),
                                from(std::move(source_root)), map_parallel(),
                                files(entries));
}

} // namespace AkRender::ShaderSetGenerator
