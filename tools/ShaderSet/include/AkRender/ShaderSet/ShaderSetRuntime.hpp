#pragma once

#include <AkRender/ShaderSet/ShaderDescriptor.hpp>
#include <AkRender/SlangJIT/SlangJIT.hpp>

#include <string_view>
#include <vector>

namespace AkRender::ShaderSet
{

/// Runtime helper that pairs an embedded shader-set view with a Slang JIT
/// session for loading modules and compiling shaders.
class ShaderSetRuntime
{
public:
  explicit ShaderSetRuntime(const VirtualFileSystemView &view) noexcept;

  const VirtualFileSystemView &view() const noexcept { return m_view; }
  SlangJITCompiler &compiler() noexcept { return m_compiler; }
  const SlangJITCompiler &compiler() const noexcept { return m_compiler; }

  [[nodiscard]] std::span<const std::byte>
  binaryBytes(const BinaryResourceDesc &resource) const noexcept;

  [[nodiscard]] std::span<const std::byte>
  moduleIR(const SlangModuleDesc &module) const noexcept;

  [[nodiscard]] std::span<const std::byte>
  shaderIR(const SlangShaderDesc &shader) const noexcept;

  [[nodiscard]] std::span<const std::byte>
  spirv(const SpirVShaderDesc &shader) const noexcept;

  [[nodiscard]] std::span<const std::byte>
  spirv(const SlangShaderDesc &shader) const noexcept;

  bool ensureModuleLoaded(const SlangModuleDesc &module);

  CompileResult compile(const SlangShaderDesc &shader);

private:
  void resetSession(const CompileOptions &options);
  bool isModuleLoaded(std::string_view manifest_name) const noexcept;

  VirtualFileSystemView m_view;
  SlangJITCompiler m_compiler;
  CompileOptions m_sessionOptions{};
  bool m_sessionReady = false;
  std::vector<std::string_view> m_loadedModules;
};

} // namespace AkRender::ShaderSet
