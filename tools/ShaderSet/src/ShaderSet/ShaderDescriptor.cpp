#include <AkRender/ShaderSet/ShaderDescriptor.hpp>

#include <AkRender/SlangJIT/SlangJIT.hpp>

#include <string>

namespace AkRender::ShaderSet
{

bool loadSlangModule(SlangJITCompiler &compiler, const SlangModuleDesc &module,
                     const void *blobData)
{
  if (module.ir.empty() || !blobData)
    return false;

  const auto bytes = moduleIRBytes(module, blobData);
  if (bytes.empty())
    return false;

  return compiler.loadModuleFromIR(module.import_name, bytes.data(),
                                   bytes.size());
}

bool loadSlangModule(SlangJITCompiler &compiler, const SlangModuleDesc &module,
                     const VirtualFileSystemView &view)
{
  return loadSlangModule(compiler, module, view.blob().data());
}

CompileResult compileSlangShader(SlangJITCompiler &compiler,
                                 const SlangShaderDesc &shader,
                                 const void *blobData)
{
  CompileResult result;

  if (!blobData)
  {
    result.diagnostic = "compileSlangShader: blobData is null";
    return result;
  }

  if (shader.ir.empty())
  {
    result.diagnostic = "compileSlangShader: shader IR record is empty";
    return result;
  }

  for (uint8_t i = 0; i < shader.num_module_deps; ++i)
  {
    if (!loadSlangModule(compiler, shader.module_deps[i], blobData))
    {
      result.diagnostic = "compileSlangShader: failed to load module '";
      result.diagnostic += shader.module_deps[i].manifest_name;
      result.diagnostic += "'";
      return result;
    }
  }

  const auto irBytes = shaderIRBytes(shader, blobData);
  if (irBytes.empty())
  {
    result.diagnostic = "compileSlangShader: shader IR bytes are empty";
    return result;
  }

  std::string shaderModuleName("__shader_");
  shaderModuleName += shader.manifest_name;

  if (!compiler.loadModuleFromIR(shaderModuleName, irBytes.data(),
                                 irBytes.size()))
  {
    result.diagnostic = "compileSlangShader: failed to load shader IR";
    return result;
  }

  return compiler.compileEntryPoint(shader.entry_point);
}

CompileResult compileSlangShader(SlangJITCompiler &compiler,
                                 const SlangShaderDesc &shader,
                                 const VirtualFileSystemView &view)
{
  return compileSlangShader(compiler, shader, view.blob().data());
}

} // namespace AkRender::ShaderSet
