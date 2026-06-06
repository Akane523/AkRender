#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <AkRender/ShaderSetGenerator/ShaderCompiler.hpp>

using AkRender::Shaders::make_manifest;
using AkRender::Shaders::Manifest;

struct ShaderSetGenerator
{
  ShaderSetGenerator() : ShaderSetGenerator(make_manifest())
  {
  }
  ShaderSetGenerator(Manifest manifest) : m_manifest(std::move(manifest))
  {
  }

  void generate();

  void generate_resource_config();

  Manifest m_manifest;
};

int main()
{
  ShaderSetGenerator generator{};
  generator.generate();
  return 0;
}