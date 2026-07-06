#include <AkRender/SlangJIT/SlangJIT.hpp>

#include <AkRender/SlangJIT/SlangCompileSession.hpp>

namespace AkRender::ShaderSet
{

struct SlangJITCompiler::Impl
{
  SlangCompileSession session;
};

SlangJITCompiler::SlangJITCompiler() : m_impl(std::make_unique<Impl>()) {}

SlangJITCompiler::~SlangJITCompiler() = default;

bool SlangJITCompiler::createSession(
    std::span<const std::filesystem::path> searchPaths,
    std::span<const PreprocessorDefine> defines, const CompileOptions &options)
{
  return m_impl->session.createSession(searchPaths, defines, options);
}

void SlangJITCompiler::destroySession() { m_impl->session.destroySession(); }

bool SlangJITCompiler::loadModuleFromIR(std::string_view moduleName,
                                        const void *data, size_t size)
{
  return m_impl->session.loadModuleFromIR(moduleName, data, size);
}

bool SlangJITCompiler::loadModuleFromSource(std::string_view moduleName,
                                            std::string_view source,
                                            std::string_view filePath)
{
  return m_impl->session.loadModuleFromSource(moduleName, source, filePath);
}

CompileResult
SlangJITCompiler::compileEntryPoint(std::string_view entryPointName)
{
  return m_impl->session.compileEntryPoint(entryPointName, Stage::Compute);
}

} // namespace AkRender::ShaderSet
