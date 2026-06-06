#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>

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

/// Child index range for directory nodes.
struct ChildRange
{
  std::size_t first_child;
  std::size_t num_children;
};

/// A single node in the flat filesystem tree.
///
/// Directories reference their children via index ranges into the owning
/// VirtualFileSystem's node array, so the whole tree is one contiguous
/// aggregate.  The node kind (File vs Directory) is determined by which
/// alternative of \c payload is active.
struct Node
{
  std::string_view                  name;    ///< component name (not full path)
  std::variant<Record, ChildRange>  payload; ///< File\u2192Record, Directory\u2192ChildRange

  constexpr bool is_file() const noexcept
  {
    return std::holds_alternative<Record>(payload);
  }
  constexpr bool is_directory() const noexcept
  {
    return std::holds_alternative<ChildRange>(payload);
  }

  constexpr const Record& data() const noexcept
  {
    return std::get<Record>(payload);
  }
  constexpr const ChildRange& children() const noexcept
  {
    return std::get<ChildRange>(payload);
  }
};

/// The root virtual filesystem for embedded resources.
///
/// Stores a flat \c Node array – the whole tree is one aggregate initialiser.
///
/// @code
///   constexpr std::byte blob_data[] = { /* binary resource data */ };
///
///   constexpr Node fs_nodes[] = {
///     // idx 0: root  "/"
///     Node{"",               NodeType::Directory, ChildRange{1, 2}},
///     // idx 1
///     Node{"manifest.json",  NodeType::File,      Record{448, 50}},
///     // idx 2: "/shaders/"
///     Node{"shaders",        NodeType::Directory, ChildRange{3, 3}},
///     // idx 3
///     Node{"triangle.spv",   NodeType::File,      Record{0, 128}},
///     // idx 4
///     Node{"quad.spv",       NodeType::File,      Record{128, 64}},
///     // idx 5: "/shaders/slang/"
///     Node{"slang",          NodeType::Directory, ChildRange{6, 1}},
///     // idx 6
///     Node{"math_utils.slang-module", NodeType::File, Record{192, 256}},
///   };
///   constexpr VirtualFileSystem fs{fs_nodes, blob_data};
/// @endcode
struct VirtualFileSystem
{
  const Node*              nodes;
  std::size_t              num_nodes;
  std::span<const std::byte> blob;

  // -- Data access ------------------------------------------------

  /// Return the bytes referenced by a \c Record as a span.
  constexpr std::span<const std::byte>
  data(const Record& rec) const noexcept
  {
    if (rec.offset + rec.size > blob.size())
      return {};
    return blob.subspan(rec.offset, rec.size);
  }

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
      auto& dir_children = dir.children();
      std::size_t child_end = dir_children.first_child + dir_children.num_children;
      std::size_t found     = child_end; // sentinel
      for (std::size_t i = dir_children.first_child; i < child_end; ++i)
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
      if (!nodes[found].is_directory())
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
    return n->data();
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
