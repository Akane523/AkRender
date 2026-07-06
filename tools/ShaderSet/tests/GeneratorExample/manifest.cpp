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
  Manifest manifest;

  manifest.embed_at("example_data", {"binary-resource.txt"},
                    Config::VirtualPath{"/example_data"});

  return manifest;
}

} // namespace AkRender::ShaderSetGenerator
