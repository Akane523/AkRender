//===------ Validate.hpp --------------------------------------------------===//
//
// Manifest validation for ShaderSetGenerator.
//
//===----------------------------------------------------------------------===//

#pragma once

#include <AkRender/ShaderSetGenerator/Manifest.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace AkRender::ShaderSetGenerator
{

/// A single manifest validation failure.
struct ValidationError
{
  std::string resource; ///< manifest resource name, empty for global errors
  std::string message;
};

/// Options controlling manifest validation.
struct ValidateOptions
{
  /// Directory containing the manifest and embedded source files.
  std::filesystem::path manifest_dir;

  /// Fail when no embedded binary resources are registered.
  bool require_embedded_resources = true;

  /// Verify that source files exist on disk.
  bool check_sources = true;
};

/// Convert a manifest resource name into a safe C++ identifier.
std::string make_cpp_identifier(std::string_view name);

/// Validate \p manifest.  Returns an empty vector on success.
std::vector<ValidationError> validate(const Manifest &manifest,
                                      ValidateOptions options);

} // namespace AkRender::ShaderSetGenerator
