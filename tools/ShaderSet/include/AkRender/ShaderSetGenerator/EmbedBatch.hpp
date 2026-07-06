//===------ EmbedBatch.hpp ------------------------------------------------===//
//
// Convenience helpers for registering embedded binary resources.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/PathMapping.hpp>

#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace AkRender::ShaderSetGenerator
{

class Manifest;

/// Determines how \c embed_tree() derives manifest resource names.
enum class TreeNamePolicy : std::uint8_t
{
  /// \c name = relative path with '/' replaced by '_'.
  RelativePath,
  /// \c name = source filename stem (duplicate stems are an error).
  Stem,
};

/// RAII helper that restores the previous VFS prefix on destruction.
class VfsPrefixScope
{
public:
  VfsPrefixScope(Manifest &manifest, Config::VirtualPath prefix);
  ~VfsPrefixScope();

  VfsPrefixScope(const VfsPrefixScope &) = delete;
  VfsPrefixScope &operator=(const VfsPrefixScope &) = delete;

private:
  Manifest *m_manifest = nullptr;
  Config::VirtualPath m_previous{};
};

/// RAII helper that restores the previous source root on destruction.
class SourceRootScope
{
public:
  SourceRootScope(Manifest &manifest, std::filesystem::path root);
  ~SourceRootScope();

  SourceRootScope(const SourceRootScope &) = delete;
  SourceRootScope &operator=(const SourceRootScope &) = delete;

private:
  Manifest *m_manifest = nullptr;
  std::filesystem::path m_previous{};
};

/// Batch builder for applying one VFS mapping recipe to many files.
class EmbedBatch
{
public:
  explicit EmbedBatch(Manifest &manifest);

  EmbedBatch &source_root(std::filesystem::path root);
  EmbedBatch &vfs_prefix(Config::VirtualPath prefix);

  EmbedBatch &map_parallel();
  EmbedBatch &map_basename();
  EmbedBatch &map_by_name();

  EmbedBatch &file(std::string name, Config::SourcePath source);
  EmbedBatch &files(
      std::initializer_list<std::pair<std::string, Config::SourcePath>> entries);
  EmbedBatch &file_at(std::string name, Config::SourcePath source,
                      Config::VirtualPath absolute_vfs);

private:
  void ensure_mapping_selected() const;
  void add_file(std::string name, Config::SourcePath source,
                Config::VfsMapping mapping);

  Manifest &m_manifest;
  std::filesystem::path m_source_root{};
  Config::VirtualPath m_vfs_prefix{};
  Config::VfsMapping m_default_mapping{};
  bool m_mapping_selected = false;
};

} // namespace AkRender::ShaderSetGenerator
