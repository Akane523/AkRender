//===------ manifest.cpp -------------------------------------------------===//
//
// ShaderSetGenerator end-to-end example with libslang build-time compilation.
//
//===----------------------------------------------------------------------===//

#include <AkRender/ShaderSetGenerator/Manifest.hpp>

using AkRender::ShaderSet::Stage;

namespace AkRender::ShaderSetGenerator
{

Manifest make_manifest()
{
  Manifest manifest;

  auto *math = manifest.add_slang_module("math");
  math->source_paths = {"shaders/math_utils.slang"};
  math->module_name = "math_utils";

  auto *vert = manifest.add_slang_shader("triangle_vert");
  vert->source_path = "shaders/triangle.slang";
  vert->entry_point = "vsMain";
  vert->stage = Stage::Vertex;
  vert->mode = Config::SlangOutputMode::SlangIR;
  vert->dependencies = {"math"};

  auto *frag = manifest.add_slang_shader("triangle_frag");
  frag->source_path = "shaders/triangle.slang";
  frag->entry_point = "fsMain";
  frag->stage = Stage::Fragment;
  frag->mode = Config::SlangOutputMode::SpirV;
  frag->dependencies = {"math"};

  auto *both = manifest.add_slang_shader("triangle_both");
  both->source_path = "shaders/triangle.slang";
  both->entry_point = "fsMain";
  both->stage = Stage::Fragment;
  both->mode = Config::SlangOutputMode::Both;
  both->dependencies = {"math"};

  return manifest;
}

} // namespace AkRender::ShaderSetGenerator
