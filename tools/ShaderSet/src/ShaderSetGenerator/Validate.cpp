//===------ Validate.cpp --------------------------------------------------===//

#include <AkRender/ShaderSetGenerator/Validate.hpp>

#include <AkRender/ShaderSetGenerator/VirtualPath.hpp>

#include <cctype>
#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace AkRender::ShaderSetGenerator
{

namespace
{

void add_error(std::vector<ValidationError> &errors, std::string resource,
               std::string message)
{
  errors.push_back({std::move(resource), std::move(message)});
}

bool is_path_prefix_conflict(std::string_view left, std::string_view right)
{
  if (left == right)
    return false;

  const auto is_prefix = [](std::string_view prefix, std::string_view path)
  {
    if (!path.starts_with(prefix))
      return false;
    return path.size() == prefix.size() || path[prefix.size()] == '/';
  };

  return is_prefix(left, right) || is_prefix(right, left);
}

fs::path resolve_manifest_source(const fs::path &path,
                                 const fs::path &manifest_dir)
{
  if (path.is_absolute())
    return path;
  return manifest_dir / path;
}

fs::path resolve_manifest_source(const Config::SourcePath &source,
                                 const fs::path &manifest_dir)
{
  return resolve_manifest_source(source.path, manifest_dir);
}

bool validate_vfs_path(std::vector<ValidationError> &errors,
                       const std::string &resource_name,
                       const Config::ResourceSeekType &seek_type,
                       std::unordered_set<std::string> &vfs_paths,
                       std::vector<std::string> &normalized_vfs_paths)
{
  if (!std::holds_alternative<Config::Embed>(seek_type))
  {
    add_error(errors, resource_name, "unsupported seek_type (expected Embed)");
    return false;
  }

  const auto &embed = std::get<Config::Embed>(seek_type);
  if (embed.virtual_path.empty())
    return true;

  const auto normalized =
      Config::normalize_vfs_path(embed.virtual_path.value);
  if (!normalized)
  {
    add_error(errors, resource_name,
              "invalid virtual path: " + normalized.error());
    return false;
  }

  if (embed.virtual_path.value != normalized->value)
  {
    add_error(errors, resource_name,
              "virtual path is not normalized (expected '" +
                  normalized->value + "')");
  }

  if (!vfs_paths.insert(normalized->value).second)
  {
    add_error(errors, resource_name,
              "duplicate virtual path '" + normalized->value + "'");
  }

  normalized_vfs_paths.push_back(normalized->value);
  return true;
}

void register_name(std::vector<ValidationError> &errors,
                   const std::string &name,
                   std::unordered_set<std::string> &names,
                   std::unordered_map<std::string, std::string> &idents)
{
  if (name.empty())
  {
    add_error(errors, {}, "resource name is empty");
    return;
  }

  if (!names.insert(name).second)
    add_error(errors, name, "duplicate resource name");

  const std::string ident = make_cpp_identifier(name);
  if (auto it = idents.find(ident); it != idents.end())
  {
    add_error(errors, name,
              "resource name collides after C++ identifier sanitization with '" +
                  it->second + "' (both become '" + ident + "')");
  }
  else
  {
    idents.emplace(ident, name);
  }
}

void check_source_file(std::vector<ValidationError> &errors,
                       const std::string &resource_name,
                       const fs::path &source_path,
                       const ValidateOptions &options)
{
  if (source_path.empty())
  {
    add_error(errors, resource_name, "source path is empty");
    return;
  }

  if (!options.check_sources || options.manifest_dir.empty())
    return;

  const fs::path abs_source =
      resolve_manifest_source(source_path, options.manifest_dir);

  std::error_code ec;
  if (!fs::exists(abs_source, ec))
  {
    add_error(errors, resource_name,
              "missing source file: " + abs_source.generic_string());
  }
  else if (!fs::is_regular_file(abs_source, ec))
  {
    add_error(errors, resource_name,
              "source path is not a regular file: " +
                  abs_source.generic_string());
  }
}

} // namespace

std::string make_cpp_identifier(std::string_view name)
{
  std::string out;
  out.reserve(name.size());

  for (char ch : name)
  {
    if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')
      out.push_back(ch);
    else
      out.push_back('_');
  }

  if (out.empty())
    return "resource";

  if (std::isdigit(static_cast<unsigned char>(out.front())))
    out.insert(out.begin(), '_');

  return out;
}

std::vector<ValidationError> validate(const Manifest &manifest,
                                      ValidateOptions options)
{
  std::vector<ValidationError> errors;

  const auto resources = manifest.binary_resources();
  const auto modules = manifest.slang_modules();
  const auto slang_shaders = manifest.slang_shaders();
  const auto spirv_shaders = manifest.spirv_shaders();

  const bool has_embeddable = !resources.empty() || !modules.empty() ||
                              !slang_shaders.empty() || !spirv_shaders.empty();
  if (options.require_embedded_resources && !has_embeddable)
  {
    add_error(errors, {}, "manifest declares no embeddable resources");
    return errors;
  }

  std::unordered_set<std::string> names;
  std::unordered_set<std::string> module_names;
  std::unordered_set<std::string> vfs_paths;
  std::unordered_map<std::string, std::string> idents;
  std::vector<std::string> normalized_vfs_paths;

  for (const Config::BinaryResource *entry : resources)
  {
    register_name(errors, entry->name, names, idents);
    check_source_file(errors, entry->name, entry->source_path.path, options);
    validate_vfs_path(errors, entry->name, entry->seek_type, vfs_paths,
                      normalized_vfs_paths);
  }

  for (const Config::SlangModule *entry : modules)
  {
    register_name(errors, entry->name, names, idents);
    module_names.insert(entry->name);

    if (entry->source_paths.empty())
    {
      add_error(errors, entry->name, "slang module has no source paths");
      continue;
    }

    for (const fs::path &source : entry->source_paths)
      check_source_file(errors, entry->name, source, options);
  }

  for (const Config::SpirV_Shader *entry : spirv_shaders)
  {
    register_name(errors, entry->name, names, idents);
    check_source_file(errors, entry->name, entry->source_path, options);
    validate_vfs_path(errors, entry->name, entry->seek_type, vfs_paths,
                      normalized_vfs_paths);
  }

  for (const Config::SlangShader *entry : slang_shaders)
  {
    register_name(errors, entry->name, names, idents);
    check_source_file(errors, entry->name, entry->source_path, options);
    validate_vfs_path(errors, entry->name, entry->seek_type, vfs_paths,
                      normalized_vfs_paths);

    for (const std::string &dep : entry->dependencies)
    {
      if (!manifest.find_slang_module(dep))
      {
        add_error(errors, entry->name,
                  "dependency references unknown slang module '" + dep + "'");
      }
    }
  }

  for (std::size_t i = 0; i < normalized_vfs_paths.size(); ++i)
  {
    for (std::size_t j = i + 1; j < normalized_vfs_paths.size(); ++j)
    {
      if (is_path_prefix_conflict(normalized_vfs_paths[i],
                                  normalized_vfs_paths[j]))
      {
        add_error(errors, {},
                  "virtual path conflict between '" + normalized_vfs_paths[i] +
                      "' and '" + normalized_vfs_paths[j] + "'");
      }
    }
  }

  return errors;
}

} // namespace AkRender::ShaderSetGenerator
