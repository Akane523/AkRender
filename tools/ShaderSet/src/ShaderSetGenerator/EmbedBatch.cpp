//===------ EmbedBatch.cpp ------------------------------------------------===//

#include <AkRender/ShaderSetGenerator/EmbedBatch.hpp>

#include <AkRender/ShaderSetGenerator/Manifest.hpp>

#include <stdexcept>
#include <utility>

namespace AkRender::ShaderSetGenerator
{

VfsPrefixScope::VfsPrefixScope(Manifest &manifest, Config::VirtualPath prefix)
    : m_manifest(&manifest), m_previous(manifest.vfs_prefix())
{
  m_manifest->set_vfs_prefix(std::move(prefix));
}

VfsPrefixScope::~VfsPrefixScope()
{
  if (m_manifest)
    m_manifest->set_vfs_prefix(std::move(m_previous));
}

SourceRootScope::SourceRootScope(Manifest &manifest,
                                 std::filesystem::path root)
    : m_manifest(&manifest), m_previous(manifest.source_root())
{
  m_manifest->set_source_root(std::move(root));
}

SourceRootScope::~SourceRootScope()
{
  if (m_manifest)
    m_manifest->set_source_root(std::move(m_previous));
}

EmbedBatch::EmbedBatch(Manifest &manifest) : m_manifest(manifest)
{
  if (const Config::VirtualPath &prefix = manifest.vfs_prefix(); !prefix.empty())
    m_vfs_prefix = prefix;
}

EmbedBatch &EmbedBatch::source_root(std::filesystem::path root)
{
  m_source_root = std::move(root);
  return *this;
}

EmbedBatch &EmbedBatch::vfs_prefix(Config::VirtualPath prefix)
{
  m_vfs_prefix = std::move(prefix);
  return *this;
}

EmbedBatch &EmbedBatch::map_parallel()
{
  m_default_mapping = Config::VfsMapping{.kind = Config::VfsMapping::Kind::Parallel};
  m_mapping_selected = true;
  return *this;
}

EmbedBatch &EmbedBatch::map_basename()
{
  m_default_mapping = Config::VfsMapping{.kind = Config::VfsMapping::Kind::Basename};
  m_mapping_selected = true;
  return *this;
}

EmbedBatch &EmbedBatch::map_by_name()
{
  m_default_mapping = Config::VfsMapping{.kind = Config::VfsMapping::Kind::ByName};
  m_mapping_selected = true;
  return *this;
}

EmbedBatch &EmbedBatch::file(std::string name, Config::SourcePath source)
{
  ensure_mapping_selected();
  add_file(std::move(name), std::move(source), m_default_mapping);
  return *this;
}

EmbedBatch &EmbedBatch::files(
    std::initializer_list<std::pair<std::string, Config::SourcePath>> entries)
{
  for (auto &[name, source] : entries)
    file(name, source);
  return *this;
}

EmbedBatch &EmbedBatch::file_at(std::string name, Config::SourcePath source,
                                Config::VirtualPath absolute_vfs)
{
  Config::VfsMapping mapping{
      .kind = Config::VfsMapping::Kind::Exact,
      .exact_path = std::move(absolute_vfs),
  };
  add_file(std::move(name), std::move(source), mapping);
  return *this;
}

void EmbedBatch::ensure_mapping_selected() const
{
  if (!m_mapping_selected)
  {
    throw std::invalid_argument(
        "embed batch requires an explicit mapping (map_parallel, map_basename, "
        "or map_by_name)");
  }
}

void EmbedBatch::add_file(std::string name, Config::SourcePath source,
                          Config::VfsMapping mapping)
{
  namespace fs = std::filesystem;

  fs::path batch_root =
      m_source_root.empty() ? fs::path(".") : fs::path(m_source_root);
  if (const fs::path manifest_root = m_manifest.source_root();
      !manifest_root.empty())
  {
    batch_root = manifest_root / batch_root;
  }

  const auto vfs = Config::resolve_vfs_path(mapping, name, source, batch_root,
                                            m_vfs_prefix);
  if (!vfs)
    throw std::invalid_argument(vfs.error());

  fs::path stored_source = source.path;
  if (!stored_source.is_absolute())
    stored_source = batch_root / stored_source;

  m_manifest.embed_at(std::move(name), Config::SourcePath{stored_source},
                      *vfs);
}

} // namespace AkRender::ShaderSetGenerator
