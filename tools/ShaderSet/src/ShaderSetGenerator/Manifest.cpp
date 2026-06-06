//===------ Manifest.cpp --------------------------------------------------===//
//
// Implements the Manifest class for building AkRender Shader Set.
//
//===----------------------------------------------------------------------===//

#include <AkRender/ShaderSetGenerator/Manifest.hpp>

#include <algorithm>
#include <string_view>
#include <vector>

namespace AkRender::ShaderSet
{

struct Manifest::Impl
{
  std::vector<Config::BinaryResource> binary_resources;
  std::vector<Config::SpirV_Shader>   spirv_shaders;
  std::vector<Config::SlangModule>    slang_modules;
  std::vector<Config::SlangShader>    slang_shaders;
};

Manifest::Manifest()
  : m_impl(std::make_unique<Impl>())
{
}

Manifest::~Manifest() = default;

Config::BinaryResource *Manifest::add_binary_resource(std::string name)
{
  auto &item = m_impl->binary_resources.emplace_back();
  item.name = std::move(name);
  return &item;
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

const Config::BinaryResource *Manifest::find_binary_resource(
    std::string_view name) const
{
  auto it = std::ranges::find(m_impl->binary_resources, name,
                              &Config::BinaryResource::name);
  if (it != m_impl->binary_resources.end())
    return &*it;
  return nullptr;
}

const Config::SpirV_Shader *Manifest::find_spirv_shader(
    std::string_view name) const
{
  auto it = std::ranges::find(m_impl->spirv_shaders, name,
                              &Config::SpirV_Shader::name);
  if (it != m_impl->spirv_shaders.end())
    return &*it;
  return nullptr;
}

const Config::SlangModule *Manifest::find_slang_module(
    std::string_view name) const
{
  auto it = std::ranges::find(m_impl->slang_modules, name,
                              &Config::SlangModule::name);
  if (it != m_impl->slang_modules.end())
    return &*it;
  return nullptr;
}

const Config::SlangShader *Manifest::find_slang_shader(
    std::string_view name) const
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

std::span<const Config::BinaryResource> Manifest::binary_resources() const
{
  return m_impl->binary_resources;
}

std::span<const Config::SpirV_Shader> Manifest::spirv_shaders() const
{
  return m_impl->spirv_shaders;
}

std::span<const Config::SlangModule> Manifest::slang_modules() const
{
  return m_impl->slang_modules;
}

std::span<const Config::SlangShader> Manifest::slang_shaders() const
{
  return m_impl->slang_shaders;
}

} // namespace AkRender::ShaderSet
