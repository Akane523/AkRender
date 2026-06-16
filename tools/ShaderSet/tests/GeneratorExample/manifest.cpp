//===------ manifest.cpp -------------------------------------------------===//
//
// Example implementation of make_manifest() for testing the Shader Set
// Generator.  Real applications provide their own manifest definition.
//
//===----------------------------------------------------------------------===//

#include <AkRender/ShaderSetGenerator/Manifest.hpp>

namespace AkRender::ShaderSetGenerator
{

Manifest make_manifest()
{
  using namespace Config;
  Manifest manifest;

  // --- Example: add a binary resource ---
  if (auto *res = manifest.add_binary_resource("example_data"))
  {
    res->source_path = "binary-resource.txt";
  }

  return manifest;
}

} // namespace AkRender::ShaderSetGenerator
