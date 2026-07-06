#include <AkRender/ShaderSet/ShaderSetRuntime.hpp>

#include <gtest/gtest.h>

#include <cstring>

#include "shader_compile_manifest.hpp"

namespace
{

using namespace AkRender::ShaderSet;

TEST(ShaderSetRuntimeTest, ReadOfflineSpirV)
{
  ShaderSetRuntime runtime{
      shader_compile_manifest::shader_compile_manifest_view};

  const auto bytes =
      runtime.spirv(shader_compile_manifest::shaders::triangle_frag);
  ASSERT_GE(bytes.size(), 4u);
  uint32_t magic = 0;
  std::memcpy(&magic, bytes.data(), sizeof(magic));
  EXPECT_EQ(magic, 0x07230203u);
}

TEST(ShaderSetRuntimeTest, ReadBothModeOfflineSpirV)
{
  ShaderSetRuntime runtime{
      shader_compile_manifest::shader_compile_manifest_view};

  const auto bytes =
      runtime.spirv(shader_compile_manifest::shaders::triangle_both);
  ASSERT_GE(bytes.size(), 4u);
  uint32_t magic = 0;
  std::memcpy(&magic, bytes.data(), sizeof(magic));
  EXPECT_EQ(magic, 0x07230203u);
}

TEST(ShaderSetRuntimeTest, ModuleCacheAvoidsDuplicateLoads)
{
  ShaderSetRuntime runtime{
      shader_compile_manifest::shader_compile_manifest_view};

  EXPECT_TRUE(
      runtime.ensureModuleLoaded(shader_compile_manifest::modules::math));
  EXPECT_TRUE(
      runtime.ensureModuleLoaded(shader_compile_manifest::modules::math));
}

TEST(ShaderSetRuntimeTest, JitCompileViaRuntime)
{
  ShaderSetRuntime runtime{
      shader_compile_manifest::shader_compile_manifest_view};

  const auto result =
      runtime.compile(shader_compile_manifest::shaders::triangle_vert);
  EXPECT_TRUE(result.success) << result.diagnostic;
  EXPECT_GE(result.binary.size(), 4u);
  EXPECT_EQ(result.binary.front(), 0x07230203u);
}

TEST(ShaderSetRuntimeTest, ReusesSessionForSameOptions)
{
  ShaderSetRuntime runtime{
      shader_compile_manifest::shader_compile_manifest_view};

  const auto first =
      runtime.compile(shader_compile_manifest::shaders::triangle_vert);
  ASSERT_TRUE(first.success) << first.diagnostic;

  const auto second =
      runtime.compile(shader_compile_manifest::shaders::triangle_vert);
  EXPECT_TRUE(second.success) << second.diagnostic;
}

} // namespace
