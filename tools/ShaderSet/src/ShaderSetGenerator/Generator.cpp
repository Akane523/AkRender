#include <AkRender/ShaderSetGenerator/Manifest.hpp>
#include <filesystem>

namespace fs = std::filesystem;

using AkRender::ShaderSetGenerator::make_manifest;
using AkRender::ShaderSetGenerator::Manifest;

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