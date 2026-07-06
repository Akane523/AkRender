//===------ SlangBuilder.cpp ----------------------------------------------===//

#include <AkRender/ShaderSetGenerator/SlangBuilder.hpp>

#include <AkRender/ShaderSetGenerator/ManifestRegister.hpp>
#include <AkRender/ShaderSetGenerator/RegisterInternals.hpp>

#include <cstdlib>
#include <exception>
#include <stdexcept>

namespace fs = std::filesystem;

namespace AkRender::ShaderSetGenerator
{

SlangBuilder::SlangBuilder(ManifestRegister reg, std::string name)
    : manifest_(reg.manifest), placement_(std::move(reg.placement)),
      name_(std::move(name))
{
}

ManifestRegister SlangBuilder::current_register() const
{
  return {.manifest = manifest_, .placement = placement_};
}

SlangBuilder::~SlangBuilder()
{
  if (!committed_ && std::uncaught_exceptions() == 0)
    std::abort();
}

SlangBuilder &SlangBuilder::source(fs::path path)
{
  source_path_ = std::move(path);
  return *this;
}

SlangBuilder &SlangBuilder::entry(std::string_view entry_point)
{
  entry_point_ = entry_point;
  return *this;
}

SlangBuilder &SlangBuilder::stage(AkRender::ShaderSet::Stage stage)
{
  stage_ = stage;
  stage_set_ = true;
  return *this;
}

SlangBuilder &SlangBuilder::options(AkRender::ShaderSet::CompileOptions opts)
{
  options_ = std::move(opts);
  return *this;
}

SlangBuilder &SlangBuilder::uses(std::string_view module_manifest_name)
{
  dependencies_.emplace_back(module_manifest_name);
  return *this;
}

SlangBuilder &SlangBuilder::uses(ModuleRef module)
{
  dependencies_.emplace_back(module.name());
  return *this;
}

SlangBuilder &SlangBuilder::uses(std::initializer_list<ModuleRef> modules)
{
  for (const ModuleRef &mod : modules)
    dependencies_.emplace_back(mod.name());
  return *this;
}

SlangBuilder &SlangBuilder::ir()
{
  mode_ = Config::SlangOutputMode::SlangIR;
  return *this;
}

SlangBuilder &SlangBuilder::spirv()
{
  mode_ = Config::SlangOutputMode::SpirV;
  return *this;
}

SlangBuilder &SlangBuilder::both()
{
  mode_ = Config::SlangOutputMode::Both;
  return *this;
}

SlangBuilder &SlangBuilder::ir_at(Config::VirtualPath path)
{
  ir_override_ = detail::require_normalized(path.value);
  return *this;
}

SlangBuilder &SlangBuilder::spv_at(Config::VirtualPath path)
{
  spv_override_ = detail::require_normalized(path.value);
  return *this;
}

SlangShaderRef SlangBuilder::commit()
{
  return commit_impl();
}

ManifestRegister SlangBuilder::commit(ManifestRegister reg)
{
  (void)reg;
  (void)commit_impl();
  return current_register();
}

SlangShaderRef SlangBuilder::commit_impl()
{
  if (committed_)
    throw std::logic_error("SlangBuilder for '" + name_ + "' already committed");

  if (source_path_.empty())
    throw std::invalid_argument("slang shader '" + name_ + "' requires source");

  if (!mode_)
    throw std::invalid_argument("slang shader '" + name_ + "' requires ir(), spirv(), or both()");

  if (!stage_set_)
    throw std::invalid_argument("slang shader '" + name_ + "' requires stage()");

  for (const std::string &dep : dependencies_)
    detail::ensure_module_dependency(manifest_, dep);

  const Config::SourcePath source{source_path_.generic_string()};

  auto *shader = manifest_.add_slang_shader(name_);
  shader->source_path =
      detail::store_source_path(manifest_, placement_, source).path;
  shader->entry_point = entry_point_;
  shader->stage = stage_;
  shader->options = options_;
  shader->mode = *mode_;
  detail::assign_unique_dependencies(shader, dependencies_);

  switch (*mode_)
  {
  case Config::SlangOutputMode::SlangIR:
    shader->ir_vfs_path = ir_override_ ? *ir_override_
                                       : detail::shader_ir_path(
                                             placement_, shader->name, source);
    break;
  case Config::SlangOutputMode::SpirV:
    shader->spv_vfs_path = spv_override_ ? *spv_override_
                                         : detail::shader_spv_path(
                                               placement_, shader->name, source);
    break;
  case Config::SlangOutputMode::Both:
    shader->ir_vfs_path = ir_override_ ? *ir_override_
                                        : detail::shader_ir_path(
                                              placement_, shader->name, source);
    shader->spv_vfs_path = spv_override_ ? *spv_override_
                                         : detail::shader_spv_path(
                                               placement_, shader->name, source);
    break;
  }

  committed_ = true;
  return SlangShaderRef{shader};
}

SlangBuilder slang(ManifestRegister reg, std::string name)
{
  return SlangBuilder{std::move(reg), std::move(name)};
}

ManifestRegister commit_slang_builder(SlangBuilder &&builder)
{
  (void)builder.commit_impl();
  return builder.current_register();
}

SlangStep::SlangStep(std::string name) : name_(std::move(name)) {}

SlangStep &SlangStep::source(fs::path path)
{
  source_path_ = std::move(path);
  return *this;
}

SlangStep &SlangStep::entry(std::string_view entry_point)
{
  entry_point_ = entry_point;
  return *this;
}

SlangStep &SlangStep::stage(AkRender::ShaderSet::Stage stage)
{
  stage_ = stage;
  stage_set_ = true;
  return *this;
}

SlangStep &SlangStep::options(AkRender::ShaderSet::CompileOptions opts)
{
  options_ = std::move(opts);
  return *this;
}

SlangStep &SlangStep::uses(std::string_view module_manifest_name)
{
  dependencies_.emplace_back(module_manifest_name);
  return *this;
}

SlangStep &SlangStep::uses(ModuleRef module)
{
  dependencies_.emplace_back(module.name());
  return *this;
}

SlangStep &SlangStep::ir()
{
  mode_ = Config::SlangOutputMode::SlangIR;
  return *this;
}

SlangStep &SlangStep::spirv()
{
  mode_ = Config::SlangOutputMode::SpirV;
  return *this;
}

SlangStep &SlangStep::both()
{
  mode_ = Config::SlangOutputMode::Both;
  return *this;
}

SlangStep &SlangStep::ir_at(Config::VirtualPath path)
{
  ir_override_ = detail::require_normalized(path.value);
  return *this;
}

SlangStep &SlangStep::spv_at(Config::VirtualPath path)
{
  spv_override_ = detail::require_normalized(path.value);
  return *this;
}

ManifestRegister SlangStep::apply(ManifestRegister reg) const
{
  SlangBuilder builder{std::move(reg), name_};
  if (!source_path_.empty())
    builder.source(source_path_);
  builder.entry(entry_point_);
  if (stage_set_)
    builder.stage(stage_);
  builder.options(options_);
  for (const std::string &dep : dependencies_)
    builder.uses(dep);
  if (mode_)
  {
    switch (*mode_)
    {
    case Config::SlangOutputMode::SlangIR:
      builder.ir();
      break;
    case Config::SlangOutputMode::SpirV:
      builder.spirv();
      break;
    case Config::SlangOutputMode::Both:
      builder.both();
      break;
    }
  }
  if (ir_override_)
    builder.ir_at(*ir_override_);
  if (spv_override_)
    builder.spv_at(*spv_override_);
  return commit_slang_builder(std::move(builder));
}

} // namespace AkRender::ShaderSetGenerator
