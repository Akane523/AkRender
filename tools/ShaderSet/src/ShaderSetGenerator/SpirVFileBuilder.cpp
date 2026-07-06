//===------ SpirVFileBuilder.cpp ------------------------------------------===//

#include <AkRender/ShaderSetGenerator/SpirVFileBuilder.hpp>

#include <AkRender/ShaderSetGenerator/ManifestRegister.hpp>
#include <AkRender/ShaderSetGenerator/RegisterInternals.hpp>

#include <cstdlib>
#include <exception>
#include <stdexcept>

namespace fs = std::filesystem;

namespace AkRender::ShaderSetGenerator
{

SpirVFileBuilder::SpirVFileBuilder(ManifestRegister reg, std::string name)
    : manifest_(reg.manifest), placement_(std::move(reg.placement)),
      name_(std::move(name))
{
}

ManifestRegister SpirVFileBuilder::current_register() const
{
  return {.manifest = manifest_, .placement = placement_};
}

SpirVFileBuilder::~SpirVFileBuilder()
{
  if (!committed_ && std::uncaught_exceptions() == 0)
    std::abort();
}

SpirVFileBuilder &SpirVFileBuilder::source(fs::path path)
{
  source_path_ = std::move(path);
  return *this;
}

SpirVFileBuilder &SpirVFileBuilder::entry(std::string_view entry_point)
{
  entry_point_ = entry_point;
  return *this;
}

SpirVFileBuilder &SpirVFileBuilder::stage(AkRender::ShaderSet::Stage stage)
{
  stage_ = stage;
  stage_set_ = true;
  return *this;
}

SpirVFileBuilder &SpirVFileBuilder::at(Config::VirtualPath vfs_path)
{
  vfs_override_ = detail::require_normalized(vfs_path.value);
  return *this;
}

SpirVShaderRef SpirVFileBuilder::commit()
{
  return commit_impl();
}

ManifestRegister SpirVFileBuilder::commit(ManifestRegister reg)
{
  (void)reg;
  (void)commit_impl();
  return current_register();
}

SpirVShaderRef SpirVFileBuilder::commit_impl()
{
  if (committed_)
    throw std::logic_error("SpirVFileBuilder for '" + name_
                           + "' already committed");

  if (source_path_.empty())
    throw std::invalid_argument("spirv_file '" + name_ + "' requires source");

  if (!stage_set_)
    throw std::invalid_argument("spirv_file '" + name_ + "' requires stage()");

  const Config::SourcePath source{source_path_.generic_string()};

  Config::VirtualPath vfs_path;
  if (vfs_override_)
    vfs_path = *vfs_override_;
  else
  {
    const auto resolved = placement_.resolve(name_, source);
    if (!resolved)
      throw std::invalid_argument(resolved.error());
    vfs_path = *resolved;
  }

  auto *shader = manifest_.add_spirv_shader(name_);
  shader->source_path =
      detail::store_source_path(manifest_, placement_, source).path;
  shader->entry_point = entry_point_;
  shader->stage = stage_;
  shader->vfs_path = vfs_path;

  committed_ = true;
  return SpirVShaderRef{shader};
}

SpirVFileBuilder spirv_file(ManifestRegister reg, std::string name)
{
  return SpirVFileBuilder{std::move(reg), std::move(name)};
}

ManifestRegister commit_spirv_file_builder(SpirVFileBuilder &&builder)
{
  (void)builder.commit_impl();
  return builder.current_register();
}

SpirVFileStep::SpirVFileStep(std::string name) : name_(std::move(name))
{
}

SpirVFileStep &SpirVFileStep::source(fs::path path)
{
  source_path_ = std::move(path);
  return *this;
}

SpirVFileStep &SpirVFileStep::entry(std::string_view entry_point)
{
  entry_point_ = entry_point;
  return *this;
}

SpirVFileStep &SpirVFileStep::stage(AkRender::ShaderSet::Stage stage)
{
  stage_ = stage;
  stage_set_ = true;
  return *this;
}

SpirVFileStep &SpirVFileStep::at(Config::VirtualPath vfs_path)
{
  vfs_override_ = detail::require_normalized(vfs_path.value);
  return *this;
}

ManifestRegister SpirVFileStep::apply(ManifestRegister reg) const
{
  SpirVFileBuilder builder{std::move(reg), name_};
  if (!source_path_.empty())
    builder.source(source_path_);
  builder.entry(entry_point_);
  if (stage_set_)
    builder.stage(stage_);
  if (vfs_override_)
    builder.at(*vfs_override_);
  return commit_spirv_file_builder(std::move(builder));
}

} // namespace AkRender::ShaderSetGenerator
