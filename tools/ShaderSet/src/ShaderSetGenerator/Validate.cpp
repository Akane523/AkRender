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

fs::path resolve_manifest_source(const Config::SourcePath &source,
                                 const fs::path &manifest_dir)
{
  if (source.path.is_absolute())
    return source.path;
  return manifest_dir / source.path;
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
  if (options.require_embedded_resources && resources.empty())
  {
    add_error(errors, {}, "no embedded binary resources in manifest");
    return errors;
  }

  std::unordered_set<std::string> names;
  std::unordered_set<std::string> vfs_paths;
  std::unordered_map<std::string, std::string> idents;
  std::vector<std::string> normalized_vfs_paths;

  for (const Config::BinaryResource *entry : resources)
  {
    const std::string &name = entry->name;

    if (name.empty())
    {
      add_error(errors, {}, "binary resource name is empty");
      continue;
    }

    if (!names.insert(name).second)
      add_error(errors, name, "duplicate resource name");

    if (entry->source_path.path.empty())
    {
      add_error(errors, name, "source path is empty");
    }
    else if (options.check_sources && !options.manifest_dir.empty())
    {
      const fs::path abs_source =
          resolve_manifest_source(entry->source_path, options.manifest_dir);

      std::error_code ec;
      if (!fs::exists(abs_source, ec))
      {
        add_error(errors, name,
                  "missing source file: " + abs_source.generic_string());
      }
      else if (!fs::is_regular_file(abs_source, ec))
      {
        add_error(errors, name,
                  "source path is not a regular file: " +
                      abs_source.generic_string());
      }
    }

    if (!std::holds_alternative<Config::Embed>(entry->seek_type))
    {
      add_error(errors, name, "unsupported seek_type (expected Embed)");
      continue;
    }

    const auto &embed = std::get<Config::Embed>(entry->seek_type);
    if (embed.virtual_path.empty())
    {
      add_error(errors, name, "virtual path is empty");
      continue;
    }

    const auto normalized =
        Config::normalize_vfs_path(embed.virtual_path.value);
    if (!normalized)
    {
      add_error(errors, name,
                "invalid virtual path: " + normalized.error());
      continue;
    }

    if (embed.virtual_path.value != normalized->value)
    {
      add_error(errors, name,
                "virtual path is not normalized (expected '" +
                    normalized->value + "')");
    }

    if (!vfs_paths.insert(normalized->value).second)
      add_error(errors, name,
                "duplicate virtual path '" + normalized->value + "'");

    normalized_vfs_paths.push_back(normalized->value);

    const std::string ident = make_cpp_identifier(name);
    if (auto it = idents.find(ident); it != idents.end())
    {
      add_error(errors, name,
                "resource name collides after C++ identifier sanitization "
                "with '" +
                    it->second + "' (both become '" + ident + "')");
    }
    else
    {
      idents.emplace(ident, name);
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
