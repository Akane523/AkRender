//===------ ModuleBuilder.cpp ---------------------------------------------===//

#include <AkRender/ShaderSetGenerator/ModuleBuilder.hpp>

#include <AkRender/ShaderSetGenerator/ManifestRegister.hpp>
#include <AkRender/ShaderSetGenerator/RegisterInternals.hpp>

#include <cstdlib>
#include <exception>
#include <stdexcept>

namespace fs = std::filesystem;

namespace AkRender::ShaderSetGenerator
{

ModuleBuilder::ModuleBuilder(ManifestRegister reg, std::string name)
    : manifest_(reg.manifest), placement_(std::move(reg.placement)),
      name_(std::move(name))
{
}

ManifestRegister ModuleBuilder::current_register() const
{
  return {.manifest = manifest_, .placement = placement_};
}

ModuleBuilder::~ModuleBuilder()
{
  if (!committed_ && std::uncaught_exceptions() == 0)
    std::abort();
}

ModuleBuilder &ModuleBuilder::source(fs::path path)
{
  sources_.push_back(std::move(path));
  return *this;
}

ModuleBuilder &ModuleBuilder::sources(std::initializer_list<fs::path> paths)
{
  sources_.assign(paths.begin(), paths.end());
  return *this;
}

ModuleBuilder &ModuleBuilder::sources(const std::vector<fs::path> &paths)
{
  sources_ = paths;
  return *this;
}

ModuleBuilder &ModuleBuilder::import_as(std::string import_name)
{
  import_name_ = std::move(import_name);
  return *this;
}

ModuleBuilder &ModuleBuilder::ir_at(Config::VirtualPath path)
{
  ir_override_ = detail::require_normalized(path.value);
  return *this;
}

ModuleRef ModuleBuilder::commit()
{
  return commit_impl();
}

ManifestRegister ModuleBuilder::commit(ManifestRegister reg)
{
  (void)reg;
  (void)commit_impl();
  return current_register();
}

ModuleRef ModuleBuilder::commit_impl()
{
  if (committed_)
    throw std::logic_error("ModuleBuilder for '" + name_ + "' already committed");

  if (sources_.empty())
    throw std::invalid_argument("module '" + name_ + "' requires at least one source");

  const Config::SourcePath primary{sources_.front().generic_string()};
  const std::string identity =
      import_name_.empty() ? name_ : import_name_;

  auto *mod = manifest_.add_slang_module(name_);
  mod->source_paths.assign(sources_.begin(), sources_.end());
  mod->module_name = identity;
  mod->ir_vfs_path = ir_override_
                         ? *ir_override_
                         : detail::module_ir_path(placement_, identity,
                                                   mod->name, primary);

  committed_ = true;
  return ModuleRef{mod};
}

ModuleBuilder module(ManifestRegister reg, std::string name)
{
  return ModuleBuilder{std::move(reg), std::move(name)};
}

ManifestRegister commit_module_builder(ModuleBuilder &&builder)
{
  (void)builder.commit_impl();
  return builder.current_register();
}

ModuleStep::ModuleStep(std::string name) : name_(std::move(name)) {}

ModuleStep &ModuleStep::source(fs::path path)
{
  sources_.push_back(std::move(path));
  return *this;
}

ModuleStep &ModuleStep::sources(std::initializer_list<fs::path> paths)
{
  sources_.assign(paths.begin(), paths.end());
  return *this;
}

ModuleStep &ModuleStep::import_as(std::string import_name)
{
  import_name_ = std::move(import_name);
  return *this;
}

ModuleStep &ModuleStep::ir_at(Config::VirtualPath path)
{
  ir_override_ = detail::require_normalized(path.value);
  return *this;
}

ManifestRegister ModuleStep::apply(ManifestRegister reg) const
{
  ModuleBuilder builder{std::move(reg), name_};
  if (!sources_.empty())
    builder.sources(sources_);
  if (!import_name_.empty())
    builder.import_as(import_name_);
  if (ir_override_)
    builder.ir_at(*ir_override_);
  return commit_module_builder(std::move(builder));
}

} // namespace AkRender::ShaderSetGenerator
