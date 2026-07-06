//===------ VirtualPath.cpp -----------------------------------------------===//

#include <AkRender/ShaderSetGenerator/VirtualPath.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace AkRender::ShaderSetGenerator::Config
{

namespace
{

bool is_valid_name_component(std::string_view component)
{
  if (component.empty() || component == "." || component == "..")
    return false;
  return component.find('\\') == std::string_view::npos &&
         component.find('/') == std::string_view::npos;
}

std::expected<std::string, std::string>
normalize_vfs_components(std::string_view raw, bool allow_trailing_slash)
{
  if (raw.find('\\') != std::string_view::npos)
    return std::unexpected("VFS path must use '/' separators");

  std::string normalized;
  normalized.reserve(raw.size() + 1);

  std::size_t pos = 0;
  if (!raw.empty() && raw.front() == '/')
    pos = 1;

  std::vector<std::string_view> components;
  while (pos <= raw.size())
  {
    const auto next = raw.find('/', pos);
    const auto component =
        raw.substr(pos, next == std::string_view::npos ? std::string_view::npos
                                                       : next - pos);
    if (next == std::string_view::npos)
    {
      if (!component.empty())
        components.push_back(component);
      break;
    }

    if (!component.empty())
      components.push_back(component);
    pos = next + 1;
  }

  if (components.empty())
    return std::unexpected("VFS path is empty");

  for (const auto component : components)
  {
    if (!is_valid_name_component(component))
      return std::unexpected("invalid VFS path component");
  }

  if (!allow_trailing_slash && raw.ends_with('/'))
    return std::unexpected("VFS file path must not end with '/'");

  normalized.push_back('/');
  for (std::size_t i = 0; i < components.size(); ++i)
  {
    if (i > 0)
      normalized.push_back('/');
    normalized.append(components[i]);
  }

  return normalized;
}

} // namespace

VirtualPath VirtualPath::from_name(std::string_view name)
{
  if (!is_valid_name_component(name))
    throw std::invalid_argument("invalid VFS name component");

  VirtualPath path;
  path.value.push_back('/');
  path.value.append(name);
  return path;
}

std::expected<VirtualPath, std::string>
normalize_vfs_path(std::string_view raw)
{
  if (raw.empty())
    return std::unexpected("VFS path is empty");

  auto normalized = normalize_vfs_components(raw, false);
  if (!normalized)
    return std::unexpected(normalized.error());

  return VirtualPath{*normalized};
}

std::expected<VirtualPath, std::string>
vfs_join(const VirtualPath &base, std::string_view leaf)
{
  if (base.empty())
    return std::unexpected("VFS prefix is empty");

  if (leaf.empty())
    return std::unexpected("VFS leaf is empty");

  if (!leaf.empty() && leaf.front() == '/')
    return std::unexpected("VFS leaf must be relative");

  if (leaf.find('\\') != std::string_view::npos)
    return std::unexpected("VFS leaf must use '/' separators");

  std::string combined = base.value;
  if (combined.back() == '/')
    combined.pop_back();
  combined.push_back('/');
  combined.append(leaf);

  return normalize_vfs_path(combined);
}

} // namespace AkRender::ShaderSetGenerator::Config
