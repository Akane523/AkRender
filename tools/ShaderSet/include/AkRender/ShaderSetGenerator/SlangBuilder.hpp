//===------ SlangBuilder.hpp ----------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/ManifestRefs.hpp>
#include <AkRender/ShaderSetGenerator/VfsPlacement.hpp>

#include <AkRender/SlangJIT/SlangJIT.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace AkRender::ShaderSetGenerator
{

struct ManifestRegister;

/// Fluent builder for one \c Config::SlangShader entry.
class SlangBuilder
{
public:
  SlangBuilder(ManifestRegister reg, std::string name);

  SlangBuilder(const SlangBuilder &) = delete;
  SlangBuilder &operator=(const SlangBuilder &) = delete;
  SlangBuilder(SlangBuilder &&) = default;
  SlangBuilder &operator=(SlangBuilder &&) = delete;

  ~SlangBuilder();

  SlangBuilder &source(std::filesystem::path path);
  SlangBuilder &entry(std::string_view entry_point);
  SlangBuilder &stage(AkRender::ShaderSet::Stage stage);
  SlangBuilder &options(AkRender::ShaderSet::CompileOptions opts);

  SlangBuilder &uses(std::string_view module_manifest_name);
  SlangBuilder &uses(ModuleRef module);
  SlangBuilder &uses(std::initializer_list<ModuleRef> modules);

  SlangBuilder &ir();
  SlangBuilder &spirv();
  SlangBuilder &both();

  SlangBuilder &ir_at(Config::VirtualPath path);
  SlangBuilder &spv_at(Config::VirtualPath path);

  [[nodiscard]] SlangShaderRef commit();
  [[nodiscard]] ManifestRegister commit(ManifestRegister reg);

private:
  friend ManifestRegister commit_slang_builder(SlangBuilder &&builder);

  [[nodiscard]] SlangShaderRef commit_impl();
  [[nodiscard]] ManifestRegister current_register() const;

  Manifest &manifest_;
  VfsPlacement placement_;
  std::string name_;
  std::filesystem::path source_path_;
  std::string entry_point_ = "main";
  AkRender::ShaderSet::Stage stage_{};
  bool stage_set_ = false;
  AkRender::ShaderSet::CompileOptions options_{};
  std::vector<std::string> dependencies_;
  std::optional<Config::SlangOutputMode> mode_;
  std::optional<Config::VirtualPath> ir_override_;
  std::optional<Config::VirtualPath> spv_override_;
  bool committed_ = false;
};

/// Pipeline step: \c reg |
/// slang("vert").source(...).stage(...).ir().uses("math")
class SlangStep
{
public:
  explicit SlangStep(std::string name);

  SlangStep &source(std::filesystem::path path);
  SlangStep &entry(std::string_view entry_point);
  SlangStep &stage(AkRender::ShaderSet::Stage stage);
  SlangStep &options(AkRender::ShaderSet::CompileOptions opts);

  SlangStep &uses(std::string_view module_manifest_name);
  SlangStep &uses(ModuleRef module);

  SlangStep &ir();
  SlangStep &spirv();
  SlangStep &both();

  SlangStep &ir_at(Config::VirtualPath path);
  SlangStep &spv_at(Config::VirtualPath path);

  [[nodiscard]] ManifestRegister apply(ManifestRegister reg) const;

private:
  std::string name_;
  std::filesystem::path source_path_;
  std::string entry_point_ = "main";
  AkRender::ShaderSet::Stage stage_{};
  bool stage_set_ = false;
  AkRender::ShaderSet::CompileOptions options_{};
  std::vector<std::string> dependencies_;
  std::optional<Config::SlangOutputMode> mode_;
  std::optional<Config::VirtualPath> ir_override_;
  std::optional<Config::VirtualPath> spv_override_;
};

[[nodiscard]] SlangBuilder slang(ManifestRegister reg, std::string name);

inline SlangStep slang(std::string name)
{
  return SlangStep{std::move(name)};
}

[[nodiscard]] ManifestRegister commit_slang_builder(SlangBuilder &&builder);

} // namespace AkRender::ShaderSetGenerator
