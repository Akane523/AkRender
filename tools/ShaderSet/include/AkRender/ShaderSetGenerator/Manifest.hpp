//===------ Manifest.hpp --------------------------------------------------===//
//
// Configures the build of an AkRender Shader Set from the source tree.
//
// The Manifest is the build-time counterpart of ShaderDescriptor.hpp:
//   - Manifest describes what to compile and how (source paths, options).
//   - ShaderDescriptor describes the final embedded data (Records into blob).
//
//===----------------------------------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/EmbedBatch.hpp>
#include <AkRender/ShaderSetGenerator/PathMapping.hpp>
#include <AkRender/ShaderSetGenerator/VirtualPath.hpp>

#include <AkRender/SlangJIT/SlangJIT.hpp>

#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace AkRender::ShaderSetGenerator
{

using AkRender::ShaderSet::CompileOptions;
using AkRender::ShaderSet::FloatMode;
using AkRender::ShaderSet::MatrixLayout;
using AkRender::ShaderSet::OptimizationLevel;
using AkRender::ShaderSet::PreprocessorDefine;
using AkRender::ShaderSet::Stage;
using AkRender::ShaderSet::TargetFormat;

namespace Config
{

/// \brief Configuration for embedding a resource into the shader set.
///
/// The resource is copied into the shader set binary at build time, and could
/// be accessed by VirutalFileSystem at runtime.
struct Embed
{
  /// Absolute normalized VFS path where the resource is embedded.
  VirtualPath virtual_path{};
};

/// \brief Determines how a resource is located at build time.
using ResourceSeekType = std::variant<std::monostate, Embed>;

/// \brief Describes a binary resource to be embedded into or referenced from
///        the shader set.
struct BinaryResource
{
  /// Unique name used to identify this resource.
  std::string name;
  /// Path to the source file on disk (absolute or relative to the manifest
  /// source root).
  SourcePath source_path;
  /// How this resource is located during the build.
  ResourceSeekType seek_type = Embed{};
};

/// \brief Describes a pre-compiled SPIR-V shader with its entry point.
struct SpirV_Shader
{
  /// Unique name used to identify this shader.
  std::string name;
  /// Path to the SPIR-V source file.
  std::filesystem::path source_path;
  /// Name of the entry-point function (e.g. \p "main").
  std::string entry_point = "main";
  /// Pipeline stage this shader targets.
  Stage stage = Stage::Vertex;
  /// How this shader is located during the build.
  ResourceSeekType seek_type = Embed{};
};

/// \brief Describes a Slang module to be compiled.
///
/// At build time slangc compiles one or more \c .slang files into a single
/// .slang-module (IR container).  Multiple sources are combined under one
/// module identity.
///
/// The module_name is what shaders use in \c import statements; it defaults to
/// \a name if not specified.
struct SlangModule
{
  /// Unique name used to identify this module in the manifest.
  std::string name;
  /// Paths to the \c .slang source file(s).
  std::vector<std::filesystem::path> source_paths;
  /// Module identity for \c -module-name (e.g. "math_utils").
  /// Defaults to \a name when empty.
  std::string module_name;
};

/// \brief Determines what output the generator should produce for a Slang
///        shader.
enum class SlangOutputMode : uint8_t
{
  /// Emit .slang-module IR only — the shader is JIT-compiled at runtime.
  SlangIR,
  /// Emit .spv only — fully offline compilation, no runtime Slang needed.
  SpirV,
  /// Emit both .spv and .slang-module IR.
  Both,
};

/// \brief Describes a shader entry point written in Slang.
///
/// At build time the generator compiles the source according to \a mode:
///   - SlangIR  →  .slang-module (for runtime JIT + reflection)
///   - SpirV    →  .spv (fully offline)
///   - Both     →  both formats
///
/// \a dependencies are module names (Config::SlangModule::name), stable across
/// any subsequent manifest mutations.
struct SlangShader
{
  /// Unique name used to identify this shader.
  std::string name;
  /// Path to the \c .slang source file.
  std::filesystem::path source_path;
  /// Name of the entry-point function (e.g. \p "main").
  std::string entry_point = "main";
  /// Pipeline stage this shader compiles to.
  Stage stage;
  /// Compile options (target, optimisation, matrix layout, etc.).
  CompileOptions options;
  /// Output mode — what the generator should produce.
  SlangOutputMode mode = SlangOutputMode::SlangIR;
  /// Names of SlangModule entries that must be imported before this shader
  /// can be compiled.  The names are compared against SlangModule::name.
  std::vector<std::string> dependencies;
  /// How this shader is located during the build.
  ResourceSeekType seek_type = Embed{};
};

} // namespace Config

/// \brief Manifest class that holds the configuration for building the shader
///        set.
class Manifest
{
public:
  /// \brief Constructs an empty manifest.
  Manifest();
  Manifest(Manifest &&) noexcept;
  ~Manifest();

  // --- Mutators -----------------------------------------------------------

  /// \brief Adds a binary resource without VFS/source configuration.
  /// \returns Pointer to the newly added entry (owned by the Manifest).
  ///          The pointer remains valid after subsequent additions.
  Config::BinaryResource *add_binary_resource(std::string name);

  /// \brief Default root for resolving relative \c SourcePath values.
  void set_source_root(std::filesystem::path root);
  [[nodiscard]] const std::filesystem::path &source_root() const;

  /// \brief Default VFS prefix used by \c embed_batch().
  void set_vfs_prefix(Config::VirtualPath prefix);
  [[nodiscard]] const Config::VirtualPath &vfs_prefix() const;

  [[nodiscard]] VfsPrefixScope push_vfs_prefix(Config::VirtualPath prefix);
  [[nodiscard]] SourceRootScope
  push_source_root(std::filesystem::path root);

  [[nodiscard]] EmbedBatch embed_batch();

  /// Embed one file at an explicit absolute VFS path.
  Config::BinaryResource *embed_at(std::string name, Config::SourcePath source,
                                   Config::VirtualPath absolute_vfs);

  /// Parallel mapping shorthand: \c vfs_prefix / relative source path.
  Manifest &embed_parallel(
      Config::VirtualPath vfs_prefix, std::filesystem::path source_root,
      std::initializer_list<std::pair<std::string, Config::SourcePath>> files);

  /// Recursively embed every regular file under \p source_dir.
  Manifest &embed_tree(std::filesystem::path source_dir,
                       Config::VirtualPath vfs_prefix,
                       TreeNamePolicy name_policy = TreeNamePolicy::RelativePath);
  /// \brief Adds a SPIR-V shader.
  /// \returns Pointer to the newly added entry (owned by the Manifest).
  ///          The pointer remains valid after subsequent additions.
  Config::SpirV_Shader *add_spirv_shader(std::string name);
  /// \brief Adds a Slang module.
  /// \returns Pointer to the newly added entry (owned by the Manifest).
  ///          The pointer remains valid after subsequent additions.
  Config::SlangModule *add_slang_module(std::string name);
  /// \brief Adds a Slang shader.
  /// \returns Pointer to the newly added entry (owned by the Manifest).
  ///          The pointer remains valid after subsequent additions.
  Config::SlangShader *add_slang_shader(std::string name);

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
  /// \brief Looks up a Slang shader by name.
  /// \returns pointer to the entry or \c nullptr if not found.
  const Config::SlangShader *find_slang_shader(std::string_view name) const;

  /// \brief Returns the number of registered binary resources.
  size_t num_binary_resources() const;
  /// \brief Returns the number of registered SPIR-V shaders.
  size_t num_spirv_shaders() const;
  /// \brief Returns the number of registered Slang modules.
  size_t num_slang_modules() const;
  /// \brief Returns the number of registered Slang shaders.
  size_t num_slang_shaders() const;

  /// \brief Returns pointers to all binary resources.
  std::vector<const Config::BinaryResource *> binary_resources() const;
  /// \brief Returns pointers to all SPIR-V shaders.
  std::vector<const Config::SpirV_Shader *> spirv_shaders() const;
  /// \brief Returns pointers to all Slang modules.
  std::vector<const Config::SlangModule *> slang_modules() const;
  /// \brief Returns pointers to all Slang shaders.
  std::vector<const Config::SlangShader *> slang_shaders() const;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

// --- External interface ------------

/// \brief Create a manifest used to generate Shader Set.
///
/// The implementation should be provided by the user of the shader set
/// generator.
Manifest make_manifest();

} // namespace AkRender::ShaderSetGenerator