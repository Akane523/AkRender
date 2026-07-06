//===------ SpirVFileBuilder.hpp ------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/ManifestRefs.hpp>
#include <AkRender/ShaderSetGenerator/VfsPlacement.hpp>

#include <AkRender/SlangJIT/SlangJIT.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace AkRender::ShaderSetGenerator
{

struct ManifestRegister;

/// Fluent builder for one disk \c Config::SpirV_Shader entry.
class SpirVFileBuilder
{
public:
  SpirVFileBuilder(ManifestRegister reg, std::string name);

  SpirVFileBuilder(const SpirVFileBuilder &) = delete;
  SpirVFileBuilder &operator=(const SpirVFileBuilder &) = delete;
  SpirVFileBuilder(SpirVFileBuilder &&) = default;
  SpirVFileBuilder &operator=(SpirVFileBuilder &&) = default;

  ~SpirVFileBuilder();

  SpirVFileBuilder &source(std::filesystem::path path);
  SpirVFileBuilder &entry(std::string_view entry_point);
  SpirVFileBuilder &stage(AkRender::ShaderSet::Stage stage);
  SpirVFileBuilder &at(Config::VirtualPath vfs_path);

  [[nodiscard]] SpirVShaderRef commit();
  [[nodiscard]] ManifestRegister commit(ManifestRegister reg);

private:
  friend ManifestRegister commit_spirv_file_builder(SpirVFileBuilder &&builder);

  [[nodiscard]] SpirVShaderRef commit_impl();
  [[nodiscard]] ManifestRegister current_register() const;

  Manifest &manifest_;
  VfsPlacement placement_;
  std::string name_;
  std::filesystem::path source_path_;
  std::string entry_point_ = "main";
  AkRender::ShaderSet::Stage stage_ = AkRender::ShaderSet::Stage::Vertex;
  bool stage_set_ = false;
  std::optional<Config::VirtualPath> vfs_override_;
  bool committed_ = false;
};

/// Pipeline step: \c reg | spirv_file("frag").source(...).stage(...).entry(...)
class SpirVFileStep
{
public:
  explicit SpirVFileStep(std::string name);

  SpirVFileStep &source(std::filesystem::path path);
  SpirVFileStep &entry(std::string_view entry_point);
  SpirVFileStep &stage(AkRender::ShaderSet::Stage stage);
  SpirVFileStep &at(Config::VirtualPath vfs_path);

  [[nodiscard]] ManifestRegister apply(ManifestRegister reg) const;

private:
  std::string name_;
  std::filesystem::path source_path_;
  std::string entry_point_ = "main";
  AkRender::ShaderSet::Stage stage_ = AkRender::ShaderSet::Stage::Vertex;
  bool stage_set_ = false;
  std::optional<Config::VirtualPath> vfs_override_;
};

[[nodiscard]] SpirVFileBuilder spirv_file(ManifestRegister reg, std::string name);

inline SpirVFileStep spirv_file(std::string name)
{
  return SpirVFileStep{std::move(name)};
}

[[nodiscard]] ManifestRegister commit_spirv_file_builder(SpirVFileBuilder &&builder);

} // namespace AkRender::ShaderSetGenerator
