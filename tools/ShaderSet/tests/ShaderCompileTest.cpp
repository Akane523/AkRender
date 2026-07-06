#include <AkRender/ShaderSet/ShaderDescriptor.hpp>
#include <AkRender/SlangJIT/SlangJIT.hpp>

#include <gtest/gtest.h>

#include <cstring>

#include "shader_compile_manifest.hpp"

namespace
{

using namespace AkRender::ShaderSet;

TEST(ShaderCompileGeneratorTest, NamedSlangModuleConstant)
{
  const SlangModuleDesc &mod = shader_compile_manifest::modules::math;
  EXPECT_EQ(mod.manifest_name, "math");
  EXPECT_EQ(mod.import_name, "math_utils");
  EXPECT_FALSE(mod.ir.empty());

  const SlangModuleDesc *lookup =
      shader_compile_manifest::find_slang_module("math");
  ASSERT_NE(lookup, nullptr);
  EXPECT_EQ(lookup->import_name, "math_utils");
}

TEST(ShaderCompileGeneratorTest, NamedSlangShaderConstant)
{
  const SlangShaderDesc &shader =
      shader_compile_manifest::shaders::triangle_vert;
  EXPECT_EQ(shader.manifest_name, "triangle_vert");
  EXPECT_EQ(shader.entry_point, "vsMain");
  EXPECT_EQ(shader.stage, Stage::Vertex);
  EXPECT_EQ(shader.num_module_deps, 1u);
  ASSERT_NE(shader.module_deps, nullptr);
  EXPECT_EQ(shader.module_deps[0].manifest_name, "math");
  EXPECT_EQ(shader.module_deps[0].import_name, "math_utils");
  EXPECT_FALSE(shader.has_offline_spirv());
}

TEST(ShaderCompileGeneratorTest, NamedSpirVShaderConstant)
{
  const SpirVShaderDesc &shader =
      shader_compile_manifest::shaders::triangle_frag;
  EXPECT_EQ(shader.manifest_name, "triangle_frag");
  EXPECT_EQ(shader.entry_point, "fsMain");
  EXPECT_EQ(shader.stage, Stage::Fragment);
  EXPECT_FALSE(shader.spirv.empty());
  EXPECT_GE(shader.spirv.size, 16u);
}

TEST(ShaderCompileGeneratorTest, BothModeShaderHasOfflineSpirV)
{
  const SlangShaderDesc &shader =
      shader_compile_manifest::shaders::triangle_both;
  EXPECT_TRUE(shader.has_offline_spirv());
  EXPECT_FALSE(shader.ir.empty());
  EXPECT_FALSE(shader.spirv.empty());
  EXPECT_EQ(shader_compile_manifest::find_spirv_shader("triangle_both"),
            nullptr);
}

TEST(ShaderCompileGeneratorTest, InternalShaderNamesNotExposed)
{
  EXPECT_EQ(shader_compile_manifest::find_slang_shader("triangle_vert_ir"),
            nullptr);
  EXPECT_EQ(shader_compile_manifest::find_spirv_shader("triangle_frag_spv"),
            nullptr);
  EXPECT_EQ(shader_compile_manifest::find_slang_module("math_utils"), nullptr);
}

TEST(ShaderCompileGeneratorTest, SpirVMagicInEmbeddedBlob)
{
  const SpirVShaderDesc &shader =
      shader_compile_manifest::shaders::triangle_frag;

  const auto data = shader_compile_manifest::shader_compile_manifest_view.read(
      shader.vfs_path);
  ASSERT_GE(data.size(), 4u);
  uint32_t magic = 0;
  std::memcpy(&magic, data.data(), sizeof(magic));
  EXPECT_EQ(magic, 0x07230203u);
}

TEST(ShaderCompileGeneratorTest, JitCompileSlangShader)
{
  const SlangShaderDesc &shader =
      shader_compile_manifest::shaders::triangle_vert;

  SlangJITCompiler compiler;
  ASSERT_TRUE(compiler.createSession({}, {}, shader.options));

  const auto result = compileSlangShader(
      compiler, shader, shader_compile_manifest::shader_compile_manifest_blob_data);
  EXPECT_TRUE(result.success) << result.diagnostic;
  EXPECT_GE(result.binary.size(), 4u);
  EXPECT_EQ(result.binary.front(), 0x07230203u);
}

} // namespace
