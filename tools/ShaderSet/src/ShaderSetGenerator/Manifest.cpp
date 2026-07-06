//===------ Manifest.cpp --------------------------------------------------===//
//
// Implements the Manifest class for building AkRender Shader Set.
//
//===----------------------------------------------------------------------===//

#include <AkRender/ShaderSetGenerator/Manifest.hpp>

#include <AkRender/ShaderSetGenerator/ManifestRegister.hpp>

#include <algorithm>
#include <deque>
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

ScopedVfsPrefix Manifest::push_vfs_prefix(Config::VirtualPath prefix)
{
  return ScopedVfsPrefix(*this, std::move(prefix));
}

ScopedSourceRoot Manifest::push_source_root(std::filesystem::path root)
{
  return ScopedSourceRoot(*this, std::move(root));
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
