#include <AkRender/ShaderSet/ShaderSetRuntime.hpp>

#include <algorithm>

namespace AkRender::ShaderSet
{

ShaderSetRuntime::ShaderSetRuntime(const VirtualFileSystemView &view) noexcept
    : m_view(view)
{
}

std::span<const std::byte>
ShaderSetRuntime::binaryBytes(const BinaryResourceDesc &resource) const noexcept
{
  return AkRender::ShaderSet::binaryBytes(resource, m_view);
}

std::span<const std::byte>
ShaderSetRuntime::moduleIR(const SlangModuleDesc &module) const noexcept
{
  return moduleIRBytes(module, m_view);
}

std::span<const std::byte>
ShaderSetRuntime::shaderIR(const SlangShaderDesc &shader) const noexcept
{
  return shaderIRBytes(shader, m_view);
}

std::span<const std::byte>
ShaderSetRuntime::spirv(const SpirVShaderDesc &shader) const noexcept
{
  return spirvBytes(shader, m_view);
}

std::span<const std::byte>
ShaderSetRuntime::spirv(const SlangShaderDesc &shader) const noexcept
{
  return shaderSpirvBytes(shader, m_view);
}

bool ShaderSetRuntime::isModuleLoaded(std::string_view manifest_name) const noexcept
{
  return std::ranges::find(m_loadedModules, manifest_name) !=
         m_loadedModules.end();
}

void ShaderSetRuntime::resetSession(const CompileOptions &options)
{
  m_compiler.destroySession();
  m_sessionReady = m_compiler.createSession({}, {}, options);
  m_sessionOptions = options;
  m_loadedModules.clear();
}

bool ShaderSetRuntime::ensureModuleLoaded(const SlangModuleDesc &module)
{
  if (isModuleLoaded(module.manifest_name))
    return true;

  if (!m_sessionReady)
    resetSession({});

  if (!loadSlangModule(m_compiler, module, m_view))
    return false;

  m_loadedModules.push_back(module.manifest_name);
  return true;
}

CompileResult ShaderSetRuntime::compile(const SlangShaderDesc &shader)
{
  if (!m_sessionReady ||
      m_sessionOptions.target_format != shader.options.target_format ||
      m_sessionOptions.optimization != shader.options.optimization ||
      m_sessionOptions.float_mode != shader.options.float_mode ||
      m_sessionOptions.matrix_layout != shader.options.matrix_layout ||
      m_sessionOptions.debug_info != shader.options.debug_info)
  {
    resetSession(shader.options);
  }

  if (!m_sessionReady)
  {
    CompileResult result;
    result.diagnostic = "ShaderSetRuntime: failed to create Slang session";
    return result;
  }

  for (uint8_t i = 0; i < shader.num_module_deps; ++i)
  {
    if (!ensureModuleLoaded(shader.module_deps[i]))
    {
      CompileResult result;
      result.diagnostic = "ShaderSetRuntime: failed to load module '";
      result.diagnostic += shader.module_deps[i].manifest_name;
      result.diagnostic += "'";
      return result;
    }
  }

  return compileSlangShader(m_compiler, shader, m_view);
}

} // namespace AkRender::ShaderSet
