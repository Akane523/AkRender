//===------ manifest.cpp -------------------------------------------------===//
//
// Example implementation of make_manifest() for testing the Shader Set
// Generator.  Real applications provide their own manifest definition.
//
//===----------------------------------------------------------------------===//

#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/ManifestRegister.hpp>

namespace AkRender::ShaderSetGenerator
{

Manifest make_manifest()
{
  Manifest manifest;

  open(manifest)
      | file_at("example_data", {"binary-resource.txt"},
                Config::VirtualPath{"/example_data"})
      | register_all();

  return manifest;
}

} // namespace AkRender::ShaderSetGenerator
