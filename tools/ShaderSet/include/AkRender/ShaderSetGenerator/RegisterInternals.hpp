//===------ RegisterInternals.hpp -----------------------------------------===//
//
// Shared helpers for manifest registration and builder commit paths.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/VfsPlacement.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace AkRender::ShaderSetGenerator::detail
{

[[nodiscard]] Config::SourcePath
store_source_path(const Manifest &manifest, const VfsPlacement &placement,
                  Config::SourcePath source);

[[nodiscard]] Config::VirtualPath require_normalized(std::string_view path);

[[nodiscard]] Config::VirtualPath
module_ir_path(const VfsPlacement &placement, std::string_view import_identity,
               std::string_view manifest_name, Config::SourcePath source);

[[nodiscard]] Config::VirtualPath
shader_ir_path(const VfsPlacement &placement, std::string_view shader_name,
               Config::SourcePath source);

[[nodiscard]] Config::VirtualPath
shader_spv_path(const VfsPlacement &placement, std::string_view shader_name,
                Config::SourcePath source);

void assign_unique_dependencies(Config::SlangShader *shader,
                                const std::vector<std::string> &dependencies);

void ensure_module_dependency(const Manifest &manifest,
                              std::string_view module_name);

} // namespace AkRender::ShaderSetGenerator::detail
