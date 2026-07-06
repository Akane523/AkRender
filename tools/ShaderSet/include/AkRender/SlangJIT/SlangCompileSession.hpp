#pragma once

#include <AkRender/SlangJIT/SlangJIT.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace AkRender::ShaderSet
{

/// Serialized Slang module IR produced at build time.
struct ModuleIRResult
{
  std::vector<std::byte> ir;
  std::string diagnostic;
  std::vector<std::filesystem::path> dependency_files;
  bool success = false;
};

/// Shared libslang session used by the runtime JIT compiler and the
/// ShaderSetGenerator build-time pipeline.
class SlangCompileSession
{
public:
  SlangCompileSession();
  ~SlangCompileSession();

  SlangCompileSession(const SlangCompileSession &) = delete;
  SlangCompileSession &operator=(const SlangCompileSession &) = delete;
  SlangCompileSession(SlangCompileSession &&) = delete;
  SlangCompileSession &operator=(SlangCompileSession &&) = delete;

  [[nodiscard]] bool hasSession() const noexcept;

  bool createSession(std::span<const std::filesystem::path> searchPaths = {},
                     std::span<const PreprocessorDefine> defines = {},
                     const CompileOptions &options = {});

  void destroySession();

  bool loadModuleFromIR(std::string_view moduleName, const void *data,
                        size_t size);

  bool loadModuleFromSource(std::string_view moduleName,
                            std::string_view source,
                            std::string_view filePath = "source.slang");

  /// Compile a \c .slang source file to serialized module IR.
  ModuleIRResult compileModuleFromFile(
      std::string_view moduleName, const std::filesystem::path &sourcePath,
      std::span<const PreprocessorDefine> defines = {});

  CompileResult compileEntryPoint(std::string_view entryPointName, Stage stage);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace AkRender::ShaderSet
