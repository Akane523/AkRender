#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/ManifestCompile.hpp>
#include <AkRender/ShaderSetGenerator/Validate.hpp>
#include <CLI/CLI.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <exception>
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

using AkRender::ShaderSetGenerator::BlobSegment;
using AkRender::ShaderSetGenerator::BlobSegmentKind;
using AkRender::ShaderSetGenerator::compile_manifest_shaders;
using AkRender::ShaderSetGenerator::ShaderCodegenData;
using AkRender::ShaderSetGenerator::make_cpp_identifier;
using AkRender::ShaderSetGenerator::make_manifest;
using AkRender::ShaderSetGenerator::Manifest;
using AkRender::ShaderSetGenerator::validate;
using AkRender::ShaderSetGenerator::ValidateOptions;
using AkRender::ShaderSetGenerator::ValidationError;
namespace Config = AkRender::ShaderSetGenerator::Config;

// ═════════════════════════════════════════════════════════════════════════
//  Internal: resource data collected from the manifest
// ═════════════════════════════════════════════════════════════════════════

/// @brief Holds the raw bytes and metadata for one blob segment.
struct ResourceInput
{
  BlobSegmentKind kind = BlobSegmentKind::Binary;
  std::string manifest_name;
  std::string vfs_path;
  std::vector<std::byte> data;
  std::size_t blob_offset = 0;
};

static const ResourceInput *
find_segment(const std::vector<ResourceInput> &resources, BlobSegmentKind kind,
             const std::string &manifest_name)
{
  for (const ResourceInput &resource : resources)
  {
    if (resource.kind == kind && resource.manifest_name == manifest_name)
      return &resource;
  }
  return nullptr;
}

static inja::json
resolve_module_records(inja::json entries,
                       const std::vector<ResourceInput> &resources)
{
  for (inja::json &entry : entries)
  {
    const std::string name = entry.at("name");
    const ResourceInput *resource =
        find_segment(resources, BlobSegmentKind::ModuleIR, name);
    if (!resource)
    {
      throw std::runtime_error("internal error: missing module IR for '" +
                               name + "'");
    }
    entry["offset"] = resource->blob_offset;
    entry["size"] = resource->data.size();
  }
  return entries;
}

static inja::json
resolve_slang_shader_records(inja::json entries,
                             const std::vector<ResourceInput> &resources)
{
  for (inja::json &entry : entries)
  {
    const std::string name = entry.at("name");
    const ResourceInput *ir =
        find_segment(resources, BlobSegmentKind::ShaderIR, name);
    if (!ir)
    {
      throw std::runtime_error("internal error: missing shader IR for '" + name +
                               "'");
    }
    entry["offset"] = ir->blob_offset;
    entry["size"] = ir->data.size();

    if (entry.contains("has_spirv") && entry.at("has_spirv").get<bool>())
    {
      const ResourceInput *spirv =
          find_segment(resources, BlobSegmentKind::ShaderSpirV, name);
      if (!spirv)
      {
        throw std::runtime_error("internal error: missing shader SPIR-V for '" +
                                 name + "'");
      }
      entry["spirv_offset"] = spirv->blob_offset;
      entry["spirv_size"] = spirv->data.size();
    }
  }
  return entries;
}

static inja::json
resolve_spirv_shader_records(inja::json entries,
                             const std::vector<ResourceInput> &resources)
{
  inja::json resolved = inja::json::array();
  for (inja::json entry : entries)
  {
    if (entry.contains("from_slang_shader") &&
        entry.at("from_slang_shader").get<bool>())
      continue;

    const std::string name = entry.at("name");
    const ResourceInput *spirv =
        find_segment(resources, BlobSegmentKind::ShaderSpirV, name);
    if (!spirv)
    {
      throw std::runtime_error("internal error: missing SPIR-V for '" + name +
                               "'");
    }
    entry["offset"] = spirv->blob_offset;
    entry["size"] = spirv->data.size();
    resolved.push_back(std::move(entry));
  }
  return resolved;
}

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
  struct ValidationFailure : std::exception
  {
    explicit ValidationFailure(std::vector<ValidationError> errors)
        : errors(std::move(errors))
    {
    }

    const char *what() const noexcept override
    {
      return "manifest validation failed";
    }

    std::vector<ValidationError> errors;
  };

  ShaderSetGenerator() : ShaderSetGenerator(make_manifest())
  {
  }
  explicit ShaderSetGenerator(Manifest manifest)
      : m_manifest(std::move(manifest))
  {
  }

  // ── main entry points ──────────────────────────────────────────────

  void generate()
  {
    using namespace std::string_literals;

    const auto validation_errors = validate(
        m_manifest,
        ValidateOptions{
            .manifest_dir = m_source_dir,
            .require_embedded_resources = true,
            .check_sources = true,
        });
    if (!validation_errors.empty())
      throw ValidationFailure{validation_errors};

    // ── Validate output directory ─────────────────────────────────────
    if (m_binary_dir.empty())
      m_binary_dir = m_source_dir;

    fs::create_directories(m_binary_dir);

    log_verbose("compiling slang modules and shaders …");
    const ShaderCodegenData shader_data =
        compile_manifest_shaders(m_manifest, m_source_dir, m_verbose_output);
    m_slang_modules = shader_data.slang_modules;
    m_slang_shaders = shader_data.slang_shaders;
    m_spirv_shaders = shader_data.spirv_shaders;

    log_verbose("collecting binary resources …");
    auto resources = collect_binary_resources();
    for (const BlobSegment &segment : shader_data.segments)
    {
      resources.push_back({
          .kind = segment.kind,
          .manifest_name = segment.manifest_name,
          .vfs_path = segment.vfs_path,
          .data = segment.data,
      });
    }

    log_verbose("concatenating binary blob …");
    const auto blob = concatenate_blob(resources);

    m_slang_modules = resolve_module_records(m_slang_modules, resources);
    m_slang_shaders =
        resolve_slang_shader_records(m_slang_shaders, resources);
    m_spirv_shaders =
        resolve_spirv_shader_records(m_spirv_shaders, resources);

    // ── build VFS nodes (flat array) ──────────────────────────────
    log_verbose("building VFS node tree …");
    const auto nodes = build_vfs_nodes(resources);

    // ── render templates ──────────────────────────────────────────
    log_verbose("rendering generated sources …");

    const auto hpp_filename = m_shader_set_name + ".hpp"s;

    render_header(nodes, resources, hpp_filename);
    render_source(blob, hpp_filename);

    // ── populate depfile ─────────────────────────────────────────
    if (!m_depfile_path.empty())
    {
      m_depfile.clear();

      const auto cpp_filename =
          hpp_filename.substr(0, hpp_filename.find_last_of('.')) + ".cpp";
      const fs::path hpp_out = m_binary_dir / hpp_filename;
      const fs::path cpp_out = m_binary_dir / cpp_filename;

      for (const Config::BinaryResource *entry : m_manifest.binary_resources())
      {
        if (!std::holds_alternative<Config::Embed>(entry->seek_type))
          continue;

        fs::path src = entry->source_path.path;
        if (src.is_relative())
          src = m_source_dir / src;

        m_depfile[hpp_out].push_back(src);
        m_depfile[cpp_out].push_back(src);
      }

      for (const fs::path &src : shader_data.source_dependencies)
        m_depfile[hpp_out].push_back(src);

      write_depfile();
    }

    if (m_verbose_output)
    {
      std::ostringstream oss;
      oss << "  generated " << (m_binary_dir / hpp_filename).string() << "  ("
          << nodes.size() << " VFS node(s), " << blob.size()
          << " blob byte(s))";
      log_verbose(oss.str());
    }
  }

  // ── data ────────────────────────────────────────────────────────────

  Manifest m_manifest;
  fs::path m_source_dir;
  fs::path m_binary_dir;
  fs::path m_template_dir;
  std::string m_shader_set_name;
  bool m_verbose_output = false;
  fs::path m_depfile_path;

  inja::json m_slang_modules = inja::json::array();
  inja::json m_slang_shaders = inja::json::array();
  inja::json m_spirv_shaders = inja::json::array();

  using depfile_t = std::map<fs::path, std::vector<fs::path>>;
  depfile_t m_depfile; ///< maps output files to their source dependencies (for build system integration)

private:
  // ── pipeline stages ─────────────────────────────────────────────────

  std::vector<ResourceInput> collect_binary_resources()
  {
    std::vector<ResourceInput> resources;

    for (const Config::BinaryResource *entry : m_manifest.binary_resources())
    {
      // Only handle Embedded resources for now
      if (!std::holds_alternative<Config::Embed>(entry->seek_type))
        continue;

      const auto &embed = std::get<Config::Embed>(entry->seek_type);

      if (embed.virtual_path.empty())
      {
        throw std::runtime_error("binary resource \"" + entry->name +
                                 "\" has no virtual path");
      }

      const std::string vpath = embed.virtual_path.value;

      fs::path abs_source = entry->source_path.path;
      if (abs_source.is_relative())
        abs_source = m_source_dir / abs_source;

      std::ifstream ifs(abs_source, std::ios::binary | std::ios::ate);
      if (!ifs)
      {
        throw std::runtime_error("cannot open binary resource \"" +
                                 entry->name + "\" at " +
                                 abs_source.string());
      }

      const auto sz = ifs.tellg();
      ifs.seekg(0, std::ios::beg);

      std::vector<std::byte> data(static_cast<std::size_t>(sz));
      ifs.read(reinterpret_cast<char *>(data.data()),
               static_cast<std::streamsize>(sz));

      resources.push_back({
          .kind = BlobSegmentKind::Binary,
          .manifest_name = entry->name,
          .vfs_path = std::move(vpath),
          .data = std::move(data),
      });

      if (m_verbose_output)
      {
        std::ostringstream oss;
        oss << "  \"" << resources.back().manifest_name << "\" → "
            << resources.back().vfs_path << "  ("
            << resources.back().data.size() << " bytes)";
        log_verbose(oss.str());
      }
    }

    return resources;
  }

  std::vector<inja::json>
  build_vfs_nodes(const std::vector<ResourceInput> &resources)
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
        trie[0].children[resources[ri].manifest_name] = trie.size();
        trie.push_back(TrieNode{
            .component = resources[ri].manifest_name,
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

  std::vector<std::byte> concatenate_blob(std::vector<ResourceInput> &resources)
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

  void render_header(const std::vector<inja::json> &nodes,
                     const std::vector<ResourceInput> &resources,
                     const std::string &hpp_filename)
  {
    auto data = make_template_data(nodes, resources);

    // ── Render via inja (Environment resolves template path) ──────────
    inja::Environment env(m_template_dir.string() + "/");
    std::string rendered = env.render_file("ShaderSet.hpp.inja", data);

    fs::path out_path = m_binary_dir / hpp_filename;
    std::ofstream ofs(out_path);
    ofs << rendered;
  }

  void render_source(const std::vector<std::byte> &blob,
                     const std::string &hpp_filename)
  {
    using namespace std::string_literals;

    auto data = make_template_data({}, {});
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

  // ── helpers ─────────────────────────────────────────────────────────

  inja::json make_template_data(const std::vector<inja::json> &nodes,
                                const std::vector<ResourceInput> &resources)
  {
    using namespace std::string_literals;

    std::string ident = make_cpp_identifier(m_shader_set_name);

    std::vector<inja::json> binary_resources;
    for (std::size_t i = 0; i < resources.size(); ++i)
    {
      const ResourceInput &resource = resources[i];
      if (resource.kind != BlobSegmentKind::Binary)
        continue;

      binary_resources.push_back({
          {"name", resource.manifest_name},
          {"ident", make_cpp_identifier(resource.manifest_name)},
          {"vfs_path", resource.vfs_path},
          {"offset", resource.blob_offset},
          {"size", resource.data.size()},
          {"index", binary_resources.size()},
      });
    }

    return {
        {"shader_set_name", m_shader_set_name},
        {"namespace_name", ident},
        {"node_count", nodes.size()},
        {"fs_var", ident + "_fs"},
        {"blob_var", ident + "_blob"},
        {"view_var", ident + "_view"},
        {"binary_table_var", ident + "_binary_resources"},
        {"binary_resource_count", binary_resources.size()},
        {"binary_resources", binary_resources},
        {"slang_module_count", m_slang_modules.size()},
        {"slang_modules", m_slang_modules},
        {"slang_shader_count", m_slang_shaders.size()},
        {"slang_shaders", m_slang_shaders},
        {"spirv_shader_count", m_spirv_shaders.size()},
        {"spirv_shaders", m_spirv_shaders},
        {"nodes", nodes},
    };
  }

  std::string read_file_to_string(const fs::path &path)
  {
    std::ifstream ifs(path);
    if (!ifs)
      return {};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
  }

  void log_verbose(std::string_view msg) const
  {
    if (m_verbose_output)
      std::cerr << msg << '\n';
  }

  void write_depfile()
  {
    if (m_depfile_path.empty())
      return;

    fs::create_directories(m_depfile_path.parent_path());

    std::ofstream ofs(m_depfile_path);
    if (!ofs)
    {
      if (m_verbose_output)
        std::cerr << "Warning: cannot write depfile at "
                  << m_depfile_path.string() << '\n';
      return;
    }

    auto quote_path = [](const fs::path &p) -> std::string
    {
      auto s = p.generic_string();
      return (s.find(' ') != std::string::npos) ? ('"' + s + '"') : s;
    };

    for (const auto &[target, deps] : m_depfile)
    {
      ofs << quote_path(target) << ':';
      for (const auto &dep : deps)
        ofs << ' ' << quote_path(dep);
      ofs << '\n';
    }
  }
};

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
      ->check(CLI::ExistingDirectory)
      ->default_val(fs::current_path());

  app.add_option("-b,--binary-dir", binary_dir,
                 "Output directory for generated shader artifacts")
      ->default_val(fs::current_path());

  app.add_option("-c,--config", config_file,
                 "Path to manifest configuration file (JSON)");

  app.add_option("-n,--name", shader_set_name,
                 "Name of the shader set (used as C++ namespace / prefix)")
      ->required();

  app.add_option("-t,--template-dir", template_dir,
                 "Directory containing inja templates")
      ->default_str("<source-dir>/template");

  app.add_flag("-v,--verbose", verbose, "Enable verbose output");

  fs::path depfile_path;
  app.add_option("--depfile", depfile_path,
                 "Path for generated dependency file (.d format). "
                 "Default: <name>.d under --binary-dir. "
                 "If relative, resolved against --binary-dir.")
      ->default_str("<name>.d");

  // Set version
  app.set_version_flag("--version", "0.1.0");

  CLI11_PARSE(app, argc, argv);

  ShaderSetGenerator generator{};
  generator.m_source_dir = std::move(source_dir);
  generator.m_binary_dir = std::move(binary_dir);
  generator.m_template_dir =
      app.count("-t") == 0 ? generator.m_source_dir / "template"
                           : std::move(template_dir);
  generator.m_shader_set_name = std::move(shader_set_name);
  generator.m_verbose_output = verbose;

  // ── resolve depfile path ──────────────────────────────────────────
  if (app.count("--depfile") == 0)
    generator.m_depfile_path =
        generator.m_binary_dir / (generator.m_shader_set_name + ".d");
  else
    generator.m_depfile_path = depfile_path.is_relative()
                                   ? generator.m_binary_dir / depfile_path
                                   : depfile_path;

  // TODO: In the future, load manifest from config_file (JSON or .cpp)
  //       For now the default constructor calls make_manifest().

  try
  {
    generator.generate();
  }
  catch (const ShaderSetGenerator::ValidationFailure &failure)
  {
    for (const ValidationError &error : failure.errors)
    {
      if (error.resource.empty())
        std::cerr << "error: " << error.message << '\n';
      else
        std::cerr << "error: [" << error.resource << "] " << error.message
                  << '\n';
    }
    return 1;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "error: " << ex.what() << '\n';
    return 1;
  }

  return 0;
}
