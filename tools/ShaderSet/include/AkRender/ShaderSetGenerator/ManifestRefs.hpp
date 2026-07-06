//===------ ManifestRefs.hpp --------------------------------------------===//
//
// Stable handles to manifest entries registered during pipeline/builder use.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/Manifest.hpp>

#include <stdexcept>
#include <string_view>

namespace AkRender::ShaderSetGenerator
{

/// Handle to a registered \c Config::SlangModule.  Only constructible via commit.
class ModuleRef
{
public:
  ModuleRef() = delete;

  [[nodiscard]] std::string_view name() const
  {
    ensure_valid();
    return entry_->name;
  }

  [[nodiscard]] std::string_view import_name() const
  {
    ensure_valid();
    return entry_->module_name.empty() ? entry_->name : entry_->module_name;
  }

  [[nodiscard]] const Config::VirtualPath &ir_vfs_path() const
  {
    ensure_valid();
    return entry_->ir_vfs_path;
  }

  [[nodiscard]] const Config::SlangModule *get() const
  {
    ensure_valid();
    return entry_;
  }

private:
  friend class ModuleBuilder;

  explicit ModuleRef(Config::SlangModule *entry) : entry_(entry)
  {
    if (entry_ == nullptr)
      throw std::logic_error("ModuleRef constructed with null entry");
  }

  void ensure_valid() const
  {
    if (entry_ == nullptr)
      throw std::logic_error("ModuleRef is invalid");
  }

  Config::SlangModule *entry_ = nullptr;
};

/// Handle to a registered \c Config::SlangShader.
class SlangShaderRef
{
public:
  SlangShaderRef() = delete;

  [[nodiscard]] std::string_view name() const
  {
    ensure_valid();
    return entry_->name;
  }

  [[nodiscard]] Config::SlangOutputMode mode() const
  {
    ensure_valid();
    return entry_->mode;
  }

  [[nodiscard]] const Config::SlangShader *get() const
  {
    ensure_valid();
    return entry_;
  }

private:
  friend class SlangBuilder;

  explicit SlangShaderRef(Config::SlangShader *entry) : entry_(entry)
  {
    if (entry_ == nullptr)
      throw std::logic_error("SlangShaderRef constructed with null entry");
  }

  void ensure_valid() const
  {
    if (entry_ == nullptr)
      throw std::logic_error("SlangShaderRef is invalid");
  }

  Config::SlangShader *entry_ = nullptr;
};

/// Handle to a registered \c Config::SpirV_Shader (disk SPIR-V).
class SpirVShaderRef
{
public:
  SpirVShaderRef() = delete;

  [[nodiscard]] std::string_view name() const
  {
    ensure_valid();
    return entry_->name;
  }

  [[nodiscard]] const Config::SpirV_Shader *get() const
  {
    ensure_valid();
    return entry_;
  }

private:
  friend class SpirVFileBuilder;

  explicit SpirVShaderRef(Config::SpirV_Shader *entry) : entry_(entry)
  {
    if (entry_ == nullptr)
      throw std::logic_error("SpirVShaderRef constructed with null entry");
  }

  void ensure_valid() const
  {
    if (entry_ == nullptr)
      throw std::logic_error("SpirVShaderRef is invalid");
  }

  Config::SpirV_Shader *entry_ = nullptr;
};

} // namespace AkRender::ShaderSetGenerator
