#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <type_traits>

namespace AkRender::Shaders
{

// ---------------------------------------------------------------------------
// Embedded virtual filesystem
//
// A simple read-only filesystem that maps path strings to embedded binary
// records.  The entire tree is stored as a flat Node[] array so it can be
// written as a single aggregate initialiser with no dynamic allocation.
// ---------------------------------------------------------------------------

/// Points into a flat embedded-data blob (offset + size).
struct Record
{
  std::size_t offset;
  std::size_t size;

  constexpr bool empty() const noexcept { return size == 0; }
};

/// Node kind discriminant.
enum class NodeType : uint8_t
{
  File,       ///< node is a file (data field is meaningful)
  Directory,  ///< node is a directory (first_child/num_children are meaningful)
};

/// A single node in the flat filesystem tree.
///
/// Directories reference their children via index ranges into the owning
/// VirtualFileSystem's node array, so the whole tree is one contiguous
/// aggregate.
struct Node
{
  std::string_view name;         ///< component name (not full path)
  NodeType         type;         ///< File or Directory
  std::size_t      first_child;  ///< Directory: start index of children
  std::size_t      num_children; ///< Directory: number of children
  Record           data;         ///< File: offset/size in the embedded blob

  constexpr bool is_file() const noexcept { return type == NodeType::File; }
  constexpr bool is_directory() const noexcept
  {
    return type == NodeType::Directory;
  }
};

/// The root virtual filesystem for embedded resources.
///
/// Stores a flat \c Node array – the whole tree is one aggregate initialiser.
///
/// @code
///   constexpr Node fs_nodes[] = {
///     // idx 0: root  "/"
///     Node{"",             NodeType::Directory, 1, 2, {}},
///     // idx 1
///     Node{"manifest.json", NodeType::File,     0, 0, {448, 50}},
///     // idx 2: "/shaders/"
///     Node{"shaders",      NodeType::Directory, 3, 3, {}},
///     // idx 3
///     Node{"triangle.spv", NodeType::File,     0, 0, {0, 128}},
///     // idx 4
///     Node{"quad.spv",     NodeType::File,     0, 0, {128, 64}},
///     // idx 5: "/shaders/slang/"
///     Node{"slang",        NodeType::Directory, 6, 1, {}},
///     // idx 6
///     Node{"math_utils.slang-module", NodeType::File, 0, 0, {192, 256}},
///   };
///   constexpr VirtualFileSystem fs{fs_nodes};
/// @endcode
struct VirtualFileSystem
{
  const Node*    nodes;
  std::size_t    num_nodes;

  // -- Lookup by std::string_view (constexpr, zero-alloc) ------------

  /// Resolve an absolute path (e.g. "/shaders/triangle.spv") to a Node.
  /// Returns nullptr if the path does not exist.
  constexpr const Node* lookup(std::string_view path) const noexcept
  {
    if (path.empty() || path.front() != '/' || num_nodes == 0)
      return nullptr;

    path.remove_prefix(1); // skip leading '/'
    if (path.empty())
      return &nodes[0]; // root

    std::size_t cur = 0; // start at root

    while (!path.empty())
    {
      auto slash = path.find('/');
      auto comp  = (slash == std::string_view::npos)
                       ? path
                       : path.substr(0, slash);

      // Search children of nodes[cur] for a node named `comp`.
      const Node& dir = nodes[cur];
      std::size_t child_end = dir.first_child + dir.num_children;
      std::size_t found     = child_end; // sentinel
      for (std::size_t i = dir.first_child; i < child_end; ++i)
      {
        if (nodes[i].name == comp)
        {
          found = i;
          break;
        }
      }

      if (found >= child_end)
        return nullptr; // component not found

      if (slash == std::string_view::npos)
        return &nodes[found]; // last component – file or directory, either is OK

      // Intermediate component – must be a directory to continue.
      if (nodes[found].type != NodeType::Directory)
        return nullptr;

      cur  = found;
      path = path.substr(slash + 1);
    }

    return &nodes[cur]; // path ended with '/'
  }

  /// Resolve an absolute path to a directory Node.
  /// Returns nullptr if the path does not exist or any component is not a
  /// directory.
  constexpr const Node*
  lookup_directory(std::string_view path) const noexcept
  {
    auto* n = lookup(path);
    if (!n || !n->is_directory())
      return nullptr;
    return n;
  }

  /// Short-hand: return the Record for a file looked up by path.
  /// Returns an empty Record if the path doesn't exist or is a directory.
  constexpr Record record(std::string_view path) const noexcept
  {
    auto* n = lookup(path);
    if (!n || !n->is_file())
      return Record{};
    return n->data;
  }

  // -- Lookup by std::filesystem::path (runtime convenience) ---------

  template <typename PathT>
    requires std::same_as<std::remove_cvref_t<PathT>, std::filesystem::path>
  const Node* lookup(PathT&& p) const noexcept
  {
    auto str = std::forward<PathT>(p).generic_string();
    return lookup(std::string_view{str});
  }

  template <typename PathT>
    requires std::same_as<std::remove_cvref_t<PathT>, std::filesystem::path>
  const Node* lookup_directory(PathT&& p) const noexcept
  {
    auto str = std::forward<PathT>(p).generic_string();
    return lookup_directory(std::string_view{str});
  }

  template <typename PathT>
    requires std::same_as<std::remove_cvref_t<PathT>, std::filesystem::path>
  Record record(PathT&& p) const noexcept
  {
    auto str = std::forward<PathT>(p).generic_string();
    return record(std::string_view{str});
  }
};

} // namespace AkRender::Shaders
