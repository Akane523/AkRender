//===------ Manifest.cpp --------------------------------------------------===//
//
// Implements the Manifest class for building AkRender Shader Set.
//
//===----------------------------------------------------------------------===//

#include <AkRender/ShaderSetGenerator/Manifest.hpp>

#include <AkRender/ShaderSetGenerator/EmbedBatch.hpp>
#include <AkRender/ShaderSetGenerator/VirtualPath.hpp>

#include <algorithm>
#include <deque>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace AkRender::ShaderSetGenerator
{

struct Manifest::Impl
{
  std::deque<Config::BinaryResource> binary_resources;
  std::deque<Config::SpirV_Shader> spirv_shaders;
  std::deque<Config::SlangModule> slang_modules;
  std::deque<Config::SlangShader> slang_shaders;
  std::filesystem::path source_root;
  Config::VirtualPath vfs_prefix;
};

Manifest::Manifest() : m_impl(std::make_unique<Impl>())
{
}

Manifest::Manifest(Manifest &&) noexcept = default;
Manifest::~Manifest() = default;

// --- Mutators --------------------------------------------------------------

Config::BinaryResource *Manifest::add_binary_resource(std::string name)
{
  auto &item = m_impl->binary_resources.emplace_back();
  item.name = std::move(name);
  return &item;
}

void Manifest::set_source_root(std::filesystem::path root)
{
  m_impl->source_root = std::move(root);
}

const std::filesystem::path &Manifest::source_root() const
{
  return m_impl->source_root;
}

void Manifest::set_vfs_prefix(Config::VirtualPath prefix)
{
  m_impl->vfs_prefix = std::move(prefix);
}

const Config::VirtualPath &Manifest::vfs_prefix() const
{
  return m_impl->vfs_prefix;
}

VfsPrefixScope Manifest::push_vfs_prefix(Config::VirtualPath prefix)
{
  return VfsPrefixScope(*this, std::move(prefix));
}

SourceRootScope Manifest::push_source_root(std::filesystem::path root)
{
  return SourceRootScope(*this, std::move(root));
}

EmbedBatch Manifest::embed_batch()
{
  return EmbedBatch(*this);
}

Config::BinaryResource *Manifest::embed_at(std::string name,
                                           Config::SourcePath source,
                                           Config::VirtualPath absolute_vfs)
{
  auto normalized = Config::normalize_vfs_path(absolute_vfs.value);
  if (!normalized)
    throw std::invalid_argument(normalized.error());

  auto *resource = add_binary_resource(std::move(name));
  resource->source_path = std::move(source);
  resource->seek_type = Config::Embed{*normalized};
  return resource;
}

Manifest &Manifest::embed_parallel(
    Config::VirtualPath vfs_prefix, std::filesystem::path source_root,
    std::initializer_list<std::pair<std::string, Config::SourcePath>> files)
{
  auto batch = embed_batch()
                   .vfs_prefix(std::move(vfs_prefix))
                   .source_root(std::move(source_root))
                   .map_parallel();
  batch.files(files);
  return *this;
}

Manifest &Manifest::embed_tree(std::filesystem::path source_dir,
                               Config::VirtualPath vfs_prefix,
                               TreeNamePolicy name_policy)
{
  namespace fs = std::filesystem;

  if (source_dir.empty())
    throw std::invalid_argument("embed_tree source_dir is empty");

  if (vfs_prefix.empty())
    throw std::invalid_argument("embed_tree requires a vfs prefix");

  fs::path tree_root = source_dir;
  if (!tree_root.is_absolute() && !m_impl->source_root.empty())
    tree_root = m_impl->source_root / tree_root;

  std::error_code ec;
  if (!fs::exists(tree_root, ec) || !fs::is_directory(tree_root, ec))
  {
    throw std::invalid_argument("embed_tree source_dir is not a directory: " +
                                tree_root.generic_string());
  }

  std::unordered_set<std::string> seen_names;
  auto batch = embed_batch()
                   .vfs_prefix(std::move(vfs_prefix))
                   .source_root(source_dir)
                   .map_parallel();

  for (const fs::directory_entry &entry :
       fs::recursive_directory_iterator(tree_root, ec))
  {
    if (ec)
    {
      throw std::invalid_argument("embed_tree failed to iterate '" +
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
    {
      name = rel.generic_string();
      std::replace(name.begin(), name.end(), '/', '_');
      break;
    }
    case TreeNamePolicy::Stem:
      name = rel.stem().generic_string();
      break;
    }

    if (name.empty())
      throw std::invalid_argument("embed_tree produced an empty resource name");

    if (!seen_names.insert(name).second)
    {
      throw std::invalid_argument("embed_tree duplicate resource name '" +
                                  name + "'");
    }

    batch.file(name, Config::SourcePath{rel});
  }

  return *this;
}

Config::SpirV_Shader *Manifest::add_spirv_shader(std::string name)
{
  auto &item = m_impl->spirv_shaders.emplace_back();
  item.name = std::move(name);
  return &item;
}

Config::SlangModule *Manifest::add_slang_module(std::string name)
{
  auto &item = m_impl->slang_modules.emplace_back();
  item.name = std::move(name);
  return &item;
}

Config::SlangShader *Manifest::add_slang_shader(std::string name)
{
  auto &item = m_impl->slang_shaders.emplace_back();
  item.name = std::move(name);
  return &item;
}

// --- Queries ----------------------------------------------------------------

const Config::BinaryResource *
Manifest::find_binary_resource(std::string_view name) const
{
  auto it = std::ranges::find(m_impl->binary_resources, name,
                              &Config::BinaryResource::name);
  if (it != m_impl->binary_resources.end())
    return &*it;
  return nullptr;
}

const Config::SpirV_Shader *
Manifest::find_spirv_shader(std::string_view name) const
{
  auto it = std::ranges::find(m_impl->spirv_shaders, name,
                              &Config::SpirV_Shader::name);
  if (it != m_impl->spirv_shaders.end())
    return &*it;
  return nullptr;
}

const Config::SlangModule *
Manifest::find_slang_module(std::string_view name) const
{
  auto it = std::ranges::find(m_impl->slang_modules, name,
                              &Config::SlangModule::name);
  if (it != m_impl->slang_modules.end())
    return &*it;
  return nullptr;
}

const Config::SlangShader *
Manifest::find_slang_shader(std::string_view name) const
{
  auto it = std::ranges::find(m_impl->slang_shaders, name,
                              &Config::SlangShader::name);
  if (it != m_impl->slang_shaders.end())
    return &*it;
  return nullptr;
}

size_t Manifest::num_binary_resources() const
{
  return m_impl->binary_resources.size();
}

size_t Manifest::num_spirv_shaders() const
{
  return m_impl->spirv_shaders.size();
}

size_t Manifest::num_slang_modules() const
{
  return m_impl->slang_modules.size();
}

size_t Manifest::num_slang_shaders() const
{
  return m_impl->slang_shaders.size();
}

std::vector<const Config::BinaryResource *> Manifest::binary_resources() const
{
  std::vector<const Config::BinaryResource *> ptrs;
  ptrs.reserve(m_impl->binary_resources.size());
  for (const auto &item : m_impl->binary_resources)
    ptrs.push_back(&item);
  return ptrs;
}

std::vector<const Config::SpirV_Shader *> Manifest::spirv_shaders() const
{
  std::vector<const Config::SpirV_Shader *> ptrs;
  ptrs.reserve(m_impl->spirv_shaders.size());
  for (const auto &item : m_impl->spirv_shaders)
    ptrs.push_back(&item);
  return ptrs;
}

std::vector<const Config::SlangModule *> Manifest::slang_modules() const
{
  std::vector<const Config::SlangModule *> ptrs;
  ptrs.reserve(m_impl->slang_modules.size());
  for (const auto &item : m_impl->slang_modules)
    ptrs.push_back(&item);
  return ptrs;
}

std::vector<const Config::SlangShader *> Manifest::slang_shaders() const
{
  std::vector<const Config::SlangShader *> ptrs;
  ptrs.reserve(m_impl->slang_shaders.size());
  for (const auto &item : m_impl->slang_shaders)
    ptrs.push_back(&item);
  return ptrs;
}

} // namespace AkRender::ShaderSetGenerator
