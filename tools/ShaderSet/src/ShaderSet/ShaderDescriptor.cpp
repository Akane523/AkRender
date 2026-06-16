#include <AkRender/ShaderSet/ShaderDescriptor.hpp>

#include <AkRender/SlangJIT/SlangJIT.hpp>

#include <cstddef>
#include <string>

namespace AkRender::ShaderSet
{

CompileResult compileSlangShader(SlangJITCompiler &compiler,
                                 const SlangShader &shader,
                                 const void *blobData)
{
  CompileResult result;

  if (!blobData)
  {
    result.diagnostic = "compileSlangShader: blobData is null";
    return result;
  }

  auto blobOfs = [blobData](const Record &r) -> const void *
  { return static_cast<const std::byte *>(blobData) + r.offset; };

  // ── Step 1: load dependent modules ─────────────────────────────────────
  for (uint8_t i = 0; i < shader.num_module_deps; ++i)
  {
    const SlangModule &dep = shader.module_deps[i];
    if (dep.data.empty())
      continue;

    if (!compiler.loadModuleFromIR(dep.name, blobOfs(dep.data), dep.data.size))
    {
      result.diagnostic = "compileSlangShader: failed to load module '";
      result.diagnostic += dep.name;
      result.diagnostic += "'";
      return result;
    }
  }

  // ── Step 2: load the shader's own .slang-module IR ────────────────────
  if (shader.ir.empty())
  {
    result.diagnostic = "compileSlangShader: shader IR record is empty";
    return result;
  }

  // Use a generated module name — the entry-point lookup doesn't depend on
  // the actual module name, just the function name.
  std::string shaderModuleName("__shader_");
  shaderModuleName += shader.entry_point;

  if (!compiler.loadModuleFromIR(shaderModuleName, blobOfs(shader.ir),
                                 shader.ir.size))
  {
    result.diagnostic = "compileSlangShader: failed to load shader IR";
    return result;
  }

  // ── Step 3: compose + link + generate SPIR-V ──────────────────────────
  return compiler.compileEntryPoint(shader.entry_point);
}

} // namespace AkRender::ShaderSet
