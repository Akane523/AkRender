//===------ manifest.cpp -------------------------------------------------===//
//
// ShaderSetGenerator end-to-end example with libslang build-time compilation.
//
//===----------------------------------------------------------------------===//

#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/ManifestRegister.hpp>

using AkRender::ShaderSet::Stage;

namespace AkRender::ShaderSetGenerator
{

Manifest make_manifest()
{
  Manifest manifest;

  open(manifest)
      | module("math")
            .sources({"shaders/math_utils.slang"})
            .import_as("math_utils")
      | slang("triangle_vert")
            .source("shaders/triangle.slang")
            .entry("vsMain")
            .stage(Stage::Vertex)
            .ir()
            .uses("math")
      | slang("triangle_frag")
            .source("shaders/triangle.slang")
            .entry("fsMain")
            .stage(Stage::Fragment)
            .spirv()
            .uses("math")
      | slang("triangle_both")
            .source("shaders/triangle.slang")
            .entry("fsMain")
            .stage(Stage::Fragment)
            .both()
            .uses("math")
      | build();

  return manifest;
}

} // namespace AkRender::ShaderSetGenerator

#include <AkRender/ShaderSetGenerator/ManifestEntry.inc>
