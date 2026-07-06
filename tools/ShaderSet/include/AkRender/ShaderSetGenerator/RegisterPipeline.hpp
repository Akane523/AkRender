//===------ RegisterPipeline.hpp ------------------------------------------===//
//
// Pipeline operator| overloads connecting ManifestRegister and builder steps.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/ModuleBuilder.hpp>
#include <AkRender/ShaderSetGenerator/SlangBuilder.hpp>
#include <AkRender/ShaderSetGenerator/SpirVFileBuilder.hpp>

#include <utility>

namespace AkRender::ShaderSetGenerator
{

[[nodiscard]] inline ManifestRegister operator|(ManifestRegister reg,
                                                const ModuleStep &step)
{
  return step.apply(std::move(reg));
}

[[nodiscard]] inline ManifestRegister operator|(ManifestRegister reg,
                                                ModuleStep &&step)
{
  return step.apply(std::move(reg));
}

[[nodiscard]] inline ManifestRegister operator|(ManifestRegister reg,
                                                const SlangStep &step)
{
  return step.apply(std::move(reg));
}

[[nodiscard]] inline ManifestRegister operator|(ManifestRegister reg,
                                                SlangStep &&step)
{
  return step.apply(std::move(reg));
}

[[nodiscard]] inline ManifestRegister operator|(ManifestRegister reg,
                                                const SpirVFileStep &step)
{
  return step.apply(std::move(reg));
}

[[nodiscard]] inline ManifestRegister operator|(ManifestRegister reg,
                                                SpirVFileStep &&step)
{
  return step.apply(std::move(reg));
}

/// Alias for \c register_all().
inline auto build()
{
  return register_all();
}

} // namespace AkRender::ShaderSetGenerator
