//===------ ModuleBuilder.hpp ---------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/ManifestRefs.hpp>
#include <AkRender/ShaderSetGenerator/VfsPlacement.hpp>

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace AkRender::ShaderSetGenerator
{

struct ManifestRegister;

/// Fluent builder for one \c Config::SlangModule entry.
class ModuleBuilder
{
public:
  ModuleBuilder(ManifestRegister reg, std::string name);

  ModuleBuilder(const ModuleBuilder &) = delete;
  ModuleBuilder &operator=(const ModuleBuilder &) = delete;
  ModuleBuilder(ModuleBuilder &&) = default;
  ModuleBuilder &operator=(ModuleBuilder &&) = default;

  ~ModuleBuilder();

  ModuleBuilder &source(std::filesystem::path path);
  ModuleBuilder &sources(std::initializer_list<std::filesystem::path> paths);
  ModuleBuilder &sources(const std::vector<std::filesystem::path> &paths);
  ModuleBuilder &import_as(std::string import_name);
  ModuleBuilder &ir_at(Config::VirtualPath path);

  [[nodiscard]] ModuleRef commit();
  [[nodiscard]] ManifestRegister commit(ManifestRegister reg);

private:
  friend ManifestRegister commit_module_builder(ModuleBuilder &&builder);

  [[nodiscard]] ModuleRef commit_impl();
  [[nodiscard]] ManifestRegister current_register() const;

  Manifest &manifest_;
  VfsPlacement placement_;
  std::string name_;
  std::vector<std::filesystem::path> sources_;
  std::string import_name_;
  std::optional<Config::VirtualPath> ir_override_;
  bool committed_ = false;
};

/// Pipeline step: \c reg | module("math").sources({...}).import_as("math_utils")
class ModuleStep
{
public:
  explicit ModuleStep(std::string name);

  ModuleStep &source(std::filesystem::path path);
  ModuleStep &sources(std::initializer_list<std::filesystem::path> paths);
  ModuleStep &import_as(std::string import_name);
  ModuleStep &ir_at(Config::VirtualPath path);

  [[nodiscard]] ManifestRegister apply(ManifestRegister reg) const;

private:
  std::string name_;
  std::vector<std::filesystem::path> sources_;
  std::string import_name_;
  std::optional<Config::VirtualPath> ir_override_;
};

[[nodiscard]] ModuleBuilder module(ManifestRegister reg, std::string name);

inline ModuleStep module(std::string name)
{
  return ModuleStep{std::move(name)};
}

[[nodiscard]] ManifestRegister commit_module_builder(ModuleBuilder &&builder);

} // namespace AkRender::ShaderSetGenerator
