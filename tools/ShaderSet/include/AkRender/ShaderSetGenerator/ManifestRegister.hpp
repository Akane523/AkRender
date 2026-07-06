//===------ ManifestRegister.hpp ------------------------------------------===//
//
// Fluent registration of manifest resources (disk embeds and shader specs).
//
//===----------------------------------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/VfsPlacement.hpp>

#include <AkRender/SlangJIT/SlangJIT.hpp>

#include <concepts>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace AkRender::ShaderSetGenerator
{

enum class TreeNamePolicy : std::uint8_t
{
  RelativePath,
  Stem,
};

/// Active manifest registration: manifest reference + current VFS layout.
struct ManifestRegister
{
  Manifest &manifest;
  VfsPlacement placement;
};

/// RAII helper that restores the previous VFS prefix on destruction.
class ScopedVfsPrefix
{
public:
  ScopedVfsPrefix(Manifest &manifest, Config::VirtualPath prefix);
  ~ScopedVfsPrefix();

  ScopedVfsPrefix(const ScopedVfsPrefix &) = delete;
  ScopedVfsPrefix &operator=(const ScopedVfsPrefix &) = delete;

private:
  Manifest *m_manifest = nullptr;
  Config::VirtualPath m_previous{};
};

/// RAII helper that restores the previous source root on destruction.
class ScopedSourceRoot
{
public:
  ScopedSourceRoot(Manifest &manifest, std::filesystem::path root);
  ~ScopedSourceRoot();

  ScopedSourceRoot(const ScopedSourceRoot &) = delete;
  ScopedSourceRoot &operator=(const ScopedSourceRoot &) = delete;

private:
  Manifest *m_manifest = nullptr;
  std::filesystem::path m_previous{};
};

[[nodiscard]] ManifestRegister open(Manifest &manifest);

[[nodiscard]] ManifestRegister from(ManifestRegister reg,
                                    std::filesystem::path source_root);

[[nodiscard]] ManifestRegister into(ManifestRegister reg,
                                    Config::VirtualPath vfs_prefix);

[[nodiscard]] ManifestRegister map_parallel(ManifestRegister reg);

[[nodiscard]] ManifestRegister file(ManifestRegister reg, std::string name,
                                    Config::SourcePath source);

[[nodiscard]] ManifestRegister files(
    ManifestRegister reg,
    std::initializer_list<std::pair<std::string, Config::SourcePath>> entries);

[[nodiscard]] ManifestRegister file_at(ManifestRegister reg, std::string name,
                                       Config::SourcePath source,
                                       Config::VirtualPath absolute_vfs);

[[nodiscard]] ManifestRegister tree(ManifestRegister reg,
                                    std::filesystem::path source_dir,
                                    Config::VirtualPath vfs_prefix,
                                    TreeNamePolicy name_policy);

[[nodiscard]] ManifestRegister
module(ManifestRegister reg, std::string name,
       std::initializer_list<std::filesystem::path> sources,
       std::string module_name = {});

[[nodiscard]] ManifestRegister
ir(ManifestRegister reg, std::string name, Config::SourcePath source,
   std::string_view entry_point, AkRender::ShaderSet::Stage stage,
   std::initializer_list<std::string_view> dependencies = {});

[[nodiscard]] ManifestRegister
spirv(ManifestRegister reg, std::string name, Config::SourcePath source,
      std::string_view entry_point, AkRender::ShaderSet::Stage stage,
      std::initializer_list<std::string_view> dependencies = {});

[[nodiscard]] ManifestRegister
both(ManifestRegister reg, std::string name, Config::SourcePath source,
     std::string_view entry_point, AkRender::ShaderSet::Stage stage,
     std::initializer_list<std::string_view> dependencies = {});

[[nodiscard]] ManifestRegister
spirv_file(ManifestRegister reg, std::string name, Config::SourcePath source,
           std::string_view entry_point, AkRender::ShaderSet::Stage stage);

/// One-shot parallel disk embed (equivalent to open | into | from |
/// map_parallel | files).
void embed_parallel(
    Manifest &manifest, Config::VirtualPath vfs_prefix,
    std::filesystem::path source_root,
    std::initializer_list<std::pair<std::string, Config::SourcePath>> files);

class ModuleStep;
class SlangStep;
class SpirVFileStep;

[[nodiscard]] ModuleStep module(std::string name);
[[nodiscard]] SlangStep slang(std::string name);
[[nodiscard]] SpirVFileStep spirv_file(std::string name);

namespace detail
{

[[nodiscard]] inline ManifestRegister pipe(ManifestRegister reg)
{
  return reg;
}

template <typename Fn, typename... Rest>
  requires requires(Fn fn, ManifestRegister reg) {
    { (reg | fn) } -> std::same_as<ManifestRegister>;
  }
[[nodiscard]] inline ManifestRegister pipe(ManifestRegister reg, Fn &&fn,
                                           Rest &&...rest)
{
  return pipe(reg | std::forward<Fn>(fn), std::forward<Rest>(rest)...);
}

} // namespace detail

template <typename Fn>
  requires requires(Fn fn, ManifestRegister reg) {
    { fn(reg) } -> std::same_as<ManifestRegister>;
  }
[[nodiscard]] ManifestRegister operator|(ManifestRegister reg, Fn fn)
{
  return fn(reg);
}

template <typename Fn>
  requires requires(Fn fn, ManifestRegister reg) {
    { fn(reg) } -> std::same_as<void>;
  }
void operator|(ManifestRegister reg, Fn &&fn)
{
  fn(reg);
}

/// Terminal pipeline step: registration complete (discards the register).
inline auto register_all()
{
  return [](ManifestRegister) {};
}

/// Apply one or more pipeline steps, then finish (void terminal).
template <typename... Fns>
  requires(sizeof...(Fns) > 0 && (requires(Fns fn, ManifestRegister reg) {
             { (reg | fn) } -> std::same_as<ManifestRegister>;
           } && ...))
inline auto register_all(Fns &&...fns)
{
  return
      [fns = std::make_tuple(std::forward<Fns>(fns)...)](ManifestRegister reg)
  {
    std::apply(
        [&reg](auto &&...steps)
        { (void)detail::pipe(reg, std::forward<decltype(steps)>(steps)...); },
        fns);
  };
}

inline auto from(std::filesystem::path source_root)
{
  return [source_root = std::move(source_root)](ManifestRegister reg)
  { return from(std::move(reg), source_root); };
}

inline auto into(Config::VirtualPath vfs_prefix)
{
  return [vfs_prefix = std::move(vfs_prefix)](ManifestRegister reg)
  { return into(std::move(reg), vfs_prefix); };
}

inline ManifestRegister map_parallel(ManifestRegister reg)
{
  reg.placement = map_parallel(std::move(reg.placement));
  return reg;
}

inline auto map_parallel()
{
  return [](ManifestRegister reg) { return map_parallel(std::move(reg)); };
}

inline auto map_basename()
{
  return [](ManifestRegister reg)
  {
    reg.placement = map_basename(std::move(reg.placement));
    return reg;
  };
}

inline auto map_by_name()
{
  return [](ManifestRegister reg)
  {
    reg.placement = map_by_name(std::move(reg.placement));
    return reg;
  };
}

inline auto
files(std::initializer_list<std::pair<std::string, Config::SourcePath>> entries)
{
  return [entries](ManifestRegister reg)
  { return files(std::move(reg), entries); };
}

inline auto file(std::string name, Config::SourcePath source)
{
  return
      [name = std::move(name), source = std::move(source)](ManifestRegister reg)
  { return file(std::move(reg), name, source); };
}

inline auto tree(std::filesystem::path source_dir,
                 Config::VirtualPath vfs_prefix,
                 TreeNamePolicy name_policy = TreeNamePolicy::RelativePath)
{
  return [source_dir = std::move(source_dir),
          vfs_prefix = std::move(vfs_prefix), name_policy](ManifestRegister reg)
  { return tree(std::move(reg), source_dir, vfs_prefix, name_policy); };
}

inline auto file_at(std::string name, Config::SourcePath source,
                    Config::VirtualPath absolute_vfs)
{
  return [name = std::move(name), source = std::move(source),
          vfs = std::move(absolute_vfs)](ManifestRegister reg)
  { return file_at(std::move(reg), name, source, vfs); };
}

/// Legacy pipe step: \c open(m) | module("math", {"a.slang"}, "math_utils") |
/// register_all()
inline auto module(std::string name,
                   std::initializer_list<std::filesystem::path> sources,
                   std::string module_name = {})
{
  return [name = std::move(name), sources = sources,
          module_name = std::move(module_name)](ManifestRegister reg)
  { return module(std::move(reg), name, sources, module_name); };
}

inline auto ir(std::string name, Config::SourcePath source,
               std::string_view entry_point, AkRender::ShaderSet::Stage stage,
               std::initializer_list<std::string_view> dependencies = {})
{
  return [name = std::move(name), source = std::move(source), entry_point,
          stage, dependencies](ManifestRegister reg)
  {
    return ir(std::move(reg), name, source, entry_point, stage, dependencies);
  };
}

inline auto spirv(std::string name, Config::SourcePath source,
                  std::string_view entry_point,
                  AkRender::ShaderSet::Stage stage,
                  std::initializer_list<std::string_view> dependencies = {})
{
  return [name = std::move(name), source = std::move(source), entry_point,
          stage, dependencies](ManifestRegister reg)
  {
    return spirv(std::move(reg), name, source, entry_point, stage,
                 dependencies);
  };
}

inline auto both(std::string name, Config::SourcePath source,
                 std::string_view entry_point, AkRender::ShaderSet::Stage stage,
                 std::initializer_list<std::string_view> dependencies = {})
{
  return [name = std::move(name), source = std::move(source), entry_point,
          stage, dependencies](ManifestRegister reg)
  {
    return both(std::move(reg), name, source, entry_point, stage, dependencies);
  };
}

inline auto spirv_file(std::string name, Config::SourcePath source,
                       std::string_view entry_point,
                       AkRender::ShaderSet::Stage stage)
{
  return [name = std::move(name), source = std::move(source), entry_point,
          stage](ManifestRegister reg)
  { return spirv_file(std::move(reg), name, source, entry_point, stage); };
}

} // namespace AkRender::ShaderSetGenerator

#include <AkRender/ShaderSetGenerator/ModuleBuilder.hpp>
#include <AkRender/ShaderSetGenerator/RegisterPipeline.hpp>
#include <AkRender/ShaderSetGenerator/SlangBuilder.hpp>
#include <AkRender/ShaderSetGenerator/SpirVFileBuilder.hpp>
