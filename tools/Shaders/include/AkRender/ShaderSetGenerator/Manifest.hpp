//===------ ShaderManifest.hpp --------------------------------------------===//
//
// Configures to build AkRender Shader Set.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace AkRender::Shaders
{

namespace Config
{

/// \brief Determines how a resource is located at build time.
enum class ResourceSeekType
{
  /// The resource content is embedded directly into the output.
  Embed,
  /// The resource is referenced via the file system at runtime.
  FileSystem,
};

/// \brief Describes a binary resource to be embedded into or referenced from
///        the shader set.
struct BinaryResource
{
  /// Unique name used to identify this resource.
  std::string name;
  /// Path to the source file on disk.
  std::filesystem::path source_path;
  /// How this resource is located during the build.
  ResourceSeekType seek_type;
};

/// \brief Describes a SPIR-V shader with its entry point.
struct SpirV_Shader
{
  /// Unique name used to identify this shader.
  std::string name;
  /// Path to the SPIR-V source file.
  std::filesystem::path source_path;
  /// Name of the entry-point function (e.g. \p "main").
  std::string entry_point;
  /// How this shader is located during the build.
  ResourceSeekType seek_type;
};

/// \brief Describes a Slang module to be compiled.
struct SlangModule
{
  /// Unique name used to identify this module.
  std::string name;
  /// Path to the \c .slang source file.
  std::filesystem::path source_path;
};

/// \brief Describes a Slang kernel (compute or other entry point) and its
///        module dependencies.
struct SlangKernel
{
  /// Unique name used to identify this kernel.
  std::string name;
  /// Path to the \c .slang source file containing the kernel.
  std::filesystem::path source_path;
  /// Name of the entry-point function (e.g. \p "computeMain").
  std::string entry_point;
  /// Modules that must be imported before this kernel can be compiled.
  std::vector<SlangModule *> dependencies;
  /// How this kernel is located during the build.
  ResourceSeekType seek_type;
};

} // namespace Config

/// \brief Manifest class that holds the configuration for building the shader
///        set.
class Manifest
{
  struct Impl;
  std::unique_ptr<Impl> m_impl;

public:
  /// \brief Constructs an empty manifest.
  Manifest();
  /// \brief Destructor.
  Manifest(Manifest &&) noexcept = default;
  ~Manifest();

  // --- Mutators -----------------------------------------------------------

  /// \brief Adds a binary resource.
  /// \returns Pointer to the newly added entry.
  Config::BinaryResource *add_binary_resource(std::string name);
  /// \brief Adds a SPIR-V shader.
  /// \returns Pointer to the newly added entry.
  Config::SpirV_Shader *add_spirv_shader(std::string name);
  /// \brief Adds a Slang module.
  /// \returns Pointer to the newly added entry.
  Config::SlangModule *add_slang_module(std::string name);
  /// \brief Adds a Slang kernel.
  /// \returns Pointer to the newly added entry.
  Config::SlangKernel *add_slang_kernel(std::string name);

  // --- Queries ------------------------------------------------------------

  /// \brief Looks up a binary resource by name.
  /// \returns pointer to the entry or \c nullptr if not found.
  const Config::BinaryResource *
  find_binary_resource(std::string_view name) const;
  /// \brief Looks up a SPIR-V shader by name.
  /// \returns pointer to the entry or \c nullptr if not found.
  const Config::SpirV_Shader *find_spirv_shader(std::string_view name) const;
  /// \brief Looks up a Slang module by name.
  /// \returns pointer to the entry or \c nullptr if not found.
  const Config::SlangModule *find_slang_module(std::string_view name) const;
  /// \brief Looks up a Slang kernel by name.
  /// \returns pointer to the entry or \c nullptr if not found.
  const Config::SlangKernel *find_slang_kernel(std::string_view name) const;

  /// \brief Returns the number of registered binary resources.
  size_t num_binary_resources() const;
  /// \brief Returns the number of registered SPIR-V shaders.
  size_t num_spirv_shaders() const;
  /// \brief Returns the number of registered Slang modules.
  size_t num_slang_modules() const;
  /// \brief Returns the number of registered Slang kernels.
  size_t num_slang_kernels() const;

  /// \brief Returns a span of all binary resources.
  std::span<const Config::BinaryResource> binary_resources() const;
  /// \brief Returns a span of all SPIR-V shaders.
  std::span<const Config::SpirV_Shader> spirv_shaders() const;
  /// \brief Returns a span of all Slang modules.
  std::span<const Config::SlangModule> slang_modules() const;
  /// \brief Returns a span of all Slang kernels.
  std::span<const Config::SlangKernel> slang_kernels() const;
};

// --- External interface ------------

/// \brief Create a manifest used to generate Shader Set.
///
/// The implementation should be provided by the user of the shader set
/// generator.
Manifest make_manifest();

} // namespace AkRender::Shaders