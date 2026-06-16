#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <CLI/CLI.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <inja/inja.hpp>

namespace fs = std::filesystem;

using AkRender::ShaderSetGenerator::make_manifest;
using AkRender::ShaderSetGenerator::Manifest;
namespace Config = AkRender::ShaderSetGenerator::Config;

// ═════════════════════════════════════════════════════════════════════════
//  Internal: resource data collected from the manifest
// ═════════════════════════════════════════════════════════════════════════

/// @brief Holds the raw bytes and metadata for one resource to be embedded.
struct ResourceInput
{
  std::string name;            ///< friendly name from manifest
  std::string vfs_path;        ///< virtual path (e.g. "/shaders/x.spv")
  std::vector<std::byte> data; ///< raw binary content
  std::size_t blob_offset = 0; ///< assigned during concatenation
};

/// @brief Trie node used to build the virtual directory tree.
struct TrieNode
{
  std::string component;                       ///< single path component
  std::map<std::string, std::size_t> children; ///< sorted by key
  bool is_file = false;
  std::size_t resource_index = 0; ///< valid iff is_file
};

// ═════════════════════════════════════════════════════════════════════════
//  Helper: flatten a path-component trie → inja::json Node array
// ═════════════════════════════════════════════════════════════════════════

/// Recursively flattens the trie rooted at \p node_idx into the output
/// json array.  Directory children are placed immediately after their
/// parent so that VFS ChildRange semantics hold.
///
/// @param trie       the whole trie (vector of TrieNode)
/// @param node_idx   index of the current node in \p trie
/// @param out_nodes  output array, appended to
/// @param resources  resource metadata (for Record offset / size)
static void flatten_dfs(const std::vector<TrieNode> &trie, std::size_t node_idx,
                        std::vector<inja::json> &out_nodes,
                        const std::vector<ResourceInput> &resources)
{
  const TrieNode &cur = trie[node_idx];

  // ── push placeholder ──────────────────────────────────────────
  const std::size_t out_idx = out_nodes.size();
  out_nodes.push_back({});

  if (cur.is_file)
  {
    // Leaf — overwrite placeholder with a file-node
    const ResourceInput &ri = resources[cur.resource_index];
    out_nodes[out_idx] = {
        {"name", cur.component},
        {"is_file", true},
        {"record", {{"offset", ri.blob_offset}, {"size", ri.data.size()}}},
    };
    return;
  }

  // ── Directory — recurse into children first ──────────────────
  const std::size_t first_child = out_nodes.size();
  for (const auto &[name, child_idx] : cur.children)
    flatten_dfs(trie, child_idx, out_nodes, resources);

  const std::size_t num_children = out_nodes.size() - first_child;

  // Overwrite placeholder
  out_nodes[out_idx] = {
      {"name", cur.component},
      {"is_file", false},
      {"children",
       {{"first_child", first_child}, {"num_children", num_children}}},
  };
}

// ═════════════════════════════════════════════════════════════════════════
//  Helper: hex formatting for blob bytes
// ═════════════════════════════════════════════════════════════════════════

static constexpr std::size_t kBytesPerLine = 12;

/// Format a contiguous byte range as C++ \c ::std::byte{ 0xXX } literals
/// suitable for direct injection into inja template output.
static std::string format_blob_bytes(const std::vector<std::byte> &blob)
{
  std::ostringstream oss;
  oss << std::hex << std::uppercase << std::setfill('0');

  for (std::size_t i = 0; i < blob.size(); ++i)
  {
    if (i % kBytesPerLine == 0)
      oss << "    ";
    oss << "::std::byte{ 0x" << std::setw(2) << static_cast<unsigned>(blob[i])
        << " }";
    if (i + 1 < blob.size())
      oss << ", ";
    if ((i + 1) % kBytesPerLine == 0 && i + 1 < blob.size())
      oss << '\n';
  }
  return oss.str();
}

// ═════════════════════════════════════════════════════════════════════════
//  ShaderSetGenerator
// ═════════════════════════════════════════════════════════════════════════

struct ShaderSetGenerator
{
  ShaderSetGenerator() : ShaderSetGenerator(make_manifest())
  {
  }
  explicit ShaderSetGenerator(Manifest manifest)
      : m_manifest(std::move(manifest))
  {
  }

  // ── main entry points ──────────────────────────────────────────────

  void generate();
  void generate_resource_config();

  // ── data ────────────────────────────────────────────────────────────

  Manifest m_manifest;
  fs::path m_source_dir;
  fs::path m_binary_dir;
  fs::path m_template_dir;
  std::string m_shader_set_name;
  bool m_verbose_output = false;

private:
  // ── pipeline stages ─────────────────────────────────────────────────

  std::vector<ResourceInput> collect_binary_resources();
  std::vector<inja::json>
  build_vfs_nodes(const std::vector<ResourceInput> &resources);
  std::vector<std::byte>
  concatenate_blob(std::vector<ResourceInput> &resources);
  void render_header(const std::vector<inja::json> &nodes,
                     const std::string &hpp_filename);
  void render_source(const std::vector<std::byte> &blob,
                     const std::string &hpp_filename);

  // ── helpers ─────────────────────────────────────────────────────────

  inja::json make_template_data(const std::vector<inja::json> &nodes);
  std::string read_file_to_string(const fs::path &path);
  void log_verbose(std::string_view msg) const;
};

// ═════════════════════════════════════════════════════════════════════
//  generate_resource_config  (stub)
// ═════════════════════════════════════════════════════════════════════

void ShaderSetGenerator::generate_resource_config()
{
  // TODO: (future) generate per-resource shader compile configurations
}

// ═════════════════════════════════════════════════════════════════════
//  generate  —  main pipeline
// ═════════════════════════════════════════════════════════════════════

void ShaderSetGenerator::generate()
{
  using namespace std::string_literals;

  // ── Validate output directory ─────────────────────────────────────
  if (m_binary_dir.empty())
    m_binary_dir = m_source_dir;

  fs::create_directories(m_binary_dir);

  // ── collect binary resources from manifest ────────────────────
  log_verbose("collecting binary resources …");
  auto resources = collect_binary_resources();
  if (resources.empty())
  {
    log_verbose("No embedded binary resources found — nothing to generate.");
    return;
  }

  // ── build VFS nodes (flat array) ──────────────────────────────
  log_verbose("building VFS node tree …");
  auto nodes = build_vfs_nodes(resources);

  // ── concatenate blob, assign offsets ──────────────────────────
  log_verbose("concatenating binary blob …");
  auto blob = concatenate_blob(resources);

  // ── render templates ──────────────────────────────────────────
  log_verbose("rendering generated sources …");

  const auto hpp_filename = m_shader_set_name + ".hpp"s;

  render_header(nodes, hpp_filename);
  render_source(blob, hpp_filename);

  if (m_verbose_output)
  {
    std::ostringstream oss;
    oss << "  generated " << (m_binary_dir / hpp_filename).string() << "  ("
        << nodes.size() << " VFS node(s), " << blob.size() << " blob byte(s))";
    log_verbose(oss.str());
  }
}

// ═════════════════════════════════════════════════════════════════════
//  collect_binary_resources
// ═════════════════════════════════════════════════════════════════════

std::vector<ResourceInput> ShaderSetGenerator::collect_binary_resources()
{
  std::vector<ResourceInput> resources;

  for (const Config::BinaryResource *entry : m_manifest.binary_resources())
  {
    // Only handle Embedded resources for now
    if (!std::holds_alternative<Config::Embed>(entry->seek_type))
      continue;

    const auto &embed = std::get<Config::Embed>(entry->seek_type);

    // Determine virtual path
    std::string vpath;
    if (!embed.virtual_path.empty())
    {
      vpath = embed.virtual_path.generic_string();
    }
    else
    {
      // Default: place at root with the resource name
      vpath = "/" + entry->name;
    }

    // Ensure path starts with '/'
    if (vpath.empty() || vpath.front() != '/')
      vpath = "/" + vpath;

    // Read source file
    fs::path abs_source = entry->source_path;
    if (abs_source.is_relative())
      abs_source = m_source_dir / abs_source;

    std::ifstream ifs(abs_source, std::ios::binary | std::ios::ate);
    if (!ifs)
    {
      if (m_verbose_output)
      {
        std::ostringstream oss;
        oss << "Warning: cannot open binary resource \"" << entry->name
            << "\" at " << abs_source.string();
        log_verbose(oss.str());
      }
      continue;
    }

    const auto sz = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    std::vector<std::byte> data(static_cast<std::size_t>(sz));
    ifs.read(reinterpret_cast<char *>(data.data()),
             static_cast<std::streamsize>(sz));

    resources.push_back({
        .name = entry->name,
        .vfs_path = std::move(vpath),
        .data = std::move(data),
    });

    if (m_verbose_output)
    {
      std::ostringstream oss;
      oss << "  \"" << resources.back().name << "\" → "
          << resources.back().vfs_path << "  (" << resources.back().data.size()
          << " bytes)";
      log_verbose(oss.str());
    }
  }

  return resources;
}

// ═════════════════════════════════════════════════════════════════════
//  build_vfs_nodes
// ═════════════════════════════════════════════════════════════════════

std::vector<inja::json>
ShaderSetGenerator::build_vfs_nodes(const std::vector<ResourceInput> &resources)
{
  if (resources.empty())
    return {};

  // ── Build trie from virtual paths ────────────────────────────────
  std::vector<TrieNode> trie;
  trie.push_back(TrieNode{.component = ""}); // root (index 0)

  for (std::size_t ri = 0; ri < resources.size(); ++ri)
  {
    std::string_view path = resources[ri].vfs_path;

    // Skip leading '/'
    if (!path.empty() && path.front() == '/')
      path.remove_prefix(1);

    if (path.empty())
    {
      // Resource at root — treat as direct child of root
      trie[0].children[resources[ri].name] = trie.size();
      trie.push_back(TrieNode{
          .component = resources[ri].name,
          .is_file = true,
          .resource_index = ri,
      });
      continue;
    }

    // Walk / create path components
    std::size_t cur = 0; // root
    std::size_t pos = 0;
    while (pos < path.size())
    {
      auto slash = path.find('/', pos);
      auto comp = (slash == std::string_view::npos)
                      ? path.substr(pos)
                      : path.substr(pos, slash - pos);

      if (comp.empty())
      {
        pos = slash + 1;
        continue;
      }

      std::string comp_str(comp);
      auto &child_map = trie[cur].children;

      if (auto it = child_map.find(comp_str); it != child_map.end())
      {
        cur = it->second;
      }
      else
      {
        // Create intermediate directory node
        std::size_t new_idx = trie.size();
        child_map[comp_str] = new_idx;
        cur = new_idx;
        trie.push_back(TrieNode{.component = comp_str});
      }

      pos = (slash == std::string_view::npos) ? path.size() : slash + 1;

      // Last component → file
      if (slash == std::string_view::npos)
      {
        trie[cur].is_file = true;
        trie[cur].resource_index = ri;
      }
    }
  }

  // ── Flatten trie to json Node array (DFS) ────────────────────────
  std::vector<inja::json> out_nodes;
  flatten_dfs(trie, 0, out_nodes, resources);

  return out_nodes;
}

// ═════════════════════════════════════════════════════════════════════
//  concatenate_blob
// ═════════════════════════════════════════════════════════════════════

std::vector<std::byte>
ShaderSetGenerator::concatenate_blob(std::vector<ResourceInput> &resources)
{
  // Compute total size
  std::size_t total = 0;
  for (const auto &r : resources)
    total += r.data.size();

  std::vector<std::byte> blob;
  blob.reserve(total);

  std::size_t offset = 0;
  for (auto &r : resources)
  {
    r.blob_offset = offset;
    blob.insert(blob.end(), r.data.begin(), r.data.end());
    offset += r.data.size();
  }

  return blob;
}

// ═════════════════════════════════════════════════════════════════════
//  render_header
// ═════════════════════════════════════════════════════════════════════

void ShaderSetGenerator::render_header(const std::vector<inja::json> &nodes,
                                       const std::string &hpp_filename)
{
  auto data = make_template_data(nodes);

  // ── Render via inja (Environment resolves template path) ──────────
  inja::Environment env(m_template_dir.string() + "/");
  std::string rendered = env.render_file("ShaderSet.hpp.inja", data);

  fs::path out_path = m_binary_dir / hpp_filename;
  std::ofstream ofs(out_path);
  ofs << rendered;
}

// ═════════════════════════════════════════════════════════════════════
//  render_source
// ═════════════════════════════════════════════════════════════════════

void ShaderSetGenerator::render_source(const std::vector<std::byte> &blob,
                                       const std::string &hpp_filename)
{
  using namespace std::string_literals;

  auto data = make_template_data({});
  // make_template_data filled in fs_var / blob_accessor etc.
  // We just need to override extra fields:
  data["hpp_filename"] = hpp_filename;
  data["blob_bytes"] = blob.empty() ? ""s : format_blob_bytes(blob);

  // ── Render via inja ───────────────────────────────────────────────
  inja::Environment env(m_template_dir.string() + "/");
  std::string rendered = env.render_file("ShaderSet.cpp.inja", data);

  auto cpp_filename =
      hpp_filename.substr(0, hpp_filename.find_last_of('.')) + ".cpp";
  fs::path out_path = m_binary_dir / cpp_filename;
  std::ofstream ofs(out_path);
  ofs << rendered;
}

// ═════════════════════════════════════════════════════════════════════
//  Template data helpers
// ═════════════════════════════════════════════════════════════════════

inja::json
ShaderSetGenerator::make_template_data(const std::vector<inja::json> &nodes)
{
  using namespace std::string_literals;

  auto safe_name = [](const std::string &raw) -> std::string
  {
    std::string out;
    for (char ch : raw)
    {
      if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')
        out.push_back(ch);
      else
        out.push_back('_');
    }
    if (out.empty())
      out = "shader_set";
    return out;
  };

  std::string ident = safe_name(m_shader_set_name);

  return {
      {"shader_set_name", m_shader_set_name},
      {"namespace_name", ident},
      {"node_count", nodes.size()},
      {"fs_var", ident + "_fs"},
      {"blob_var", ident + "_blob"},
      {"view_var",        ident + "_view"},
      {"nodes", nodes},
  };
}

std::string ShaderSetGenerator::read_file_to_string(const fs::path &path)
{
  std::ifstream ifs(path);
  if (!ifs)
    return {};
  std::ostringstream oss;
  oss << ifs.rdbuf();
  return oss.str();
}

void ShaderSetGenerator::log_verbose(std::string_view msg) const
{
  if (m_verbose_output)
    std::cerr << msg << '\n';
}

// ═════════════════════════════════════════════════════════════════════
//  main
// ═════════════════════════════════════════════════════════════════════

int main(int argc, char **argv)
{
  CLI::App app{
      "AkRender Shader Set Generator — offline shader code generation tool"};

  fs::path source_dir;
  fs::path binary_dir;
  fs::path config_file;
  fs::path template_dir;
  std::string shader_set_name;
  bool verbose = false;

  app.add_option("-s,--source-dir", source_dir,
                 "Source directory containing shader files")
      ->check(CLI::ExistingDirectory);

  app.add_option("-b,--binary-dir", binary_dir,
                 "Output directory for generated shader artifacts");

  app.add_option("-c,--config", config_file,
                 "Path to manifest configuration file (JSON)");

  app.add_option("-n,--name", shader_set_name,
                 "Name of the shader set (used as C++ namespace / prefix)")
      ->required();

  app.add_option("-t,--template-dir", template_dir,
                 "Directory containing inja templates");

  app.add_flag("-v,--verbose", verbose, "Enable verbose output");

  // Set version
  app.set_version_flag("--version", "0.1.0");

  CLI11_PARSE(app, argc, argv);

  ShaderSetGenerator generator{};
  generator.m_source_dir = source_dir.empty() ? fs::current_path() : source_dir;
  generator.m_binary_dir = binary_dir.empty() ? fs::current_path() : binary_dir;
  generator.m_template_dir =
      template_dir.empty() ? generator.m_source_dir / "template" : template_dir;
  generator.m_shader_set_name = std::move(shader_set_name);
  generator.m_verbose_output = verbose;

  // TODO: In the future, load manifest from config_file (JSON or .cpp)
  //       For now the default constructor calls make_manifest().

  generator.generate();

  return 0;
}
