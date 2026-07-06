#include <AkRender/ShaderSet/ShaderDescriptor.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>

#include "shader_compile_manifest.hpp"
#include "test_manifest.hpp"

namespace
{

using namespace AkRender::ShaderSet;

TEST(ShaderDescriptorTest, RecordBytesViaView)
{
  const auto &view = shader_compile_manifest::shader_compile_manifest_view;
  const auto bytes =
      recordBytes(shader_compile_manifest::modules::math.ir, view);
  const auto viaPath =
      view.read("/shaders/slang/math_utils.slang-module");
  ASSERT_FALSE(bytes.empty());
  ASSERT_EQ(bytes.size(), viaPath.size());
  EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), viaPath.begin()));
}

TEST(ShaderDescriptorTest, BinaryBytesViaView)
{
  const auto &view = test_manifest::test_manifest_view;
  const auto bytes = binaryBytes(test_manifest::resources::example_data, view);
  ASSERT_EQ(bytes.size(), 21u);

  const std::string_view content(reinterpret_cast<const char *>(bytes.data()),
                                 bytes.size());
  EXPECT_EQ(content, "Hello Binary Resource");
}

TEST(ShaderDescriptorTest, ShaderSpirvBytesForBothMode)
{
  const auto &view = shader_compile_manifest::shader_compile_manifest_view;
  const auto &shader = shader_compile_manifest::shaders::triangle_both;
  ASSERT_TRUE(shader.has_offline_spirv());

  const auto bytes = shaderSpirvBytes(shader, view);
  ASSERT_GE(bytes.size(), 4u);
  uint32_t magic = 0;
  std::memcpy(&magic, bytes.data(), sizeof(magic));
  EXPECT_EQ(magic, 0x07230203u);

  const auto alias = spirvBytes(shader, view);
  ASSERT_EQ(bytes.size(), alias.size());
  EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), alias.begin()));
}

TEST(ShaderDescriptorTest, JitShaderHasIrButNoOfflineSpirv)
{
  const auto &view = shader_compile_manifest::shader_compile_manifest_view;
  const auto &shader = shader_compile_manifest::shaders::triangle_vert;
  EXPECT_FALSE(shaderIRBytes(shader, view).empty());
  EXPECT_TRUE(shaderSpirvBytes(shader, view).empty());
}

TEST(ShaderDescriptorTest, OfflineSpirVShaderBytes)
{
  const auto &view = shader_compile_manifest::shader_compile_manifest_view;
  const auto &shader = shader_compile_manifest::shaders::triangle_frag;
  EXPECT_FALSE(spirvBytes(shader, view).empty());
}

TEST(ShaderDescriptorTest, CompileSlangShaderViaView)
{
  const auto &view = shader_compile_manifest::shader_compile_manifest_view;
  const auto &shader = shader_compile_manifest::shaders::triangle_vert;

  SlangJITCompiler compiler;
  ASSERT_TRUE(compiler.createSession({}, {}, shader.options));

  const auto result = compileSlangShader(compiler, shader, view);
  EXPECT_TRUE(result.success) << result.diagnostic;
  EXPECT_GE(result.binary.size(), 4u);
}

} // namespace
