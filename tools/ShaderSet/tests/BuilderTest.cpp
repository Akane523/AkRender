#include <AkRender/ShaderSetGenerator/ManifestRegister.hpp>

#include <gtest/gtest.h>

using AkRender::ShaderSet::Stage;

namespace AkRender::ShaderSetGenerator
{
namespace
{

TEST(BuilderTest, ModuleRefValidAfterPipelineCommit)
{
  Manifest manifest;
  open(manifest)
      | module("math").sources({"math.slang"}).import_as("math_utils")
      | build();

  const auto *mod = manifest.find_slang_module("math");
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(mod->module_name, "math_utils");
}

TEST(BuilderTest, SlangBuilderPipelineRegistersShader)
{
  Manifest manifest;
  open(manifest) | module("math").sources({"math.slang"})
      | slang("vert")
            .source("tri.slang")
            .entry("vsMain")
            .stage(Stage::Vertex)
            .ir()
            .uses("math")
      | build();

  const auto *shader = manifest.find_slang_shader("vert");
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->mode, Config::SlangOutputMode::SlangIR);
  ASSERT_EQ(shader->dependencies.size(), 1U);
  EXPECT_EQ(shader->dependencies.front(), "math");
}

TEST(BuilderTest, UsesModuleRefInImperativeCommit)
{
  Manifest manifest;
  ManifestRegister reg = open(manifest);
  const ModuleRef math = module(reg, "math")
                             .sources({"math.slang"})
                             .import_as("math_utils")
                             .commit();

  EXPECT_EQ(math.name(), "math");
  EXPECT_EQ(math.import_name(), "math_utils");

  (void)slang(reg, "vert")
      .source("tri.slang")
      .entry("vsMain")
      .stage(Stage::Vertex)
      .ir()
      .uses(math)
      .commit(reg);

  const auto *shader = manifest.find_slang_shader("vert");
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->dependencies.front(), "math");
}

TEST(BuilderTest, UsesUnknownModuleThrowsAtCommit)
{
  Manifest manifest;
  EXPECT_THROW(open(manifest)
                   | slang("vert")
                         .source("tri.slang")
                         .entry("vsMain")
                         .stage(Stage::Vertex)
                         .ir()
                         .uses("missing")
                   | build(),
               std::invalid_argument);
}

TEST(BuilderTest, UncommittedModuleBuilderAbortsOnDestruction)
{
  Manifest manifest;
  EXPECT_DEATH(
      {
        ManifestRegister reg = open(manifest);
        (void)module(reg, "math");
      },
      ".*");
}

TEST(BuilderTest, DoubleCommitThrows)
{
  Manifest manifest;
  ModuleBuilder builder = module(open(manifest), "math");
  builder.sources({"math.slang"});
  (void)builder.commit();
  EXPECT_THROW((void)builder.commit(), std::logic_error);
}

TEST(BuilderTest, BuilderInterleavesWithFileRegistration)
{
  Manifest manifest;
  open(manifest) | into({"/assets"}) | map_parallel()
      | file("config", {"config.bin"}) | module("math").sources({"math.slang"})
      | build();

  EXPECT_EQ(manifest.num_binary_resources(), 1U);
  EXPECT_EQ(manifest.num_slang_modules(), 1U);
}

TEST(BuilderTest, SpirvAndBothModesRegisterExpectedPaths)
{
  Manifest manifest;
  open(manifest) | module("math").sources({"math.slang"})
      | slang("frag")
            .source("frag.slang")
            .entry("fsMain")
            .stage(Stage::Fragment)
            .spirv()
            .uses("math")
      | slang("combo")
            .source("combo.slang")
            .entry("main")
            .stage(Stage::Compute)
            .both()
            .uses("math")
      | build();

  const auto *frag = manifest.find_slang_shader("frag");
  ASSERT_NE(frag, nullptr);
  EXPECT_EQ(frag->mode, Config::SlangOutputMode::SpirV);
  EXPECT_TRUE(frag->ir_vfs_path.empty());
  EXPECT_EQ(frag->spv_vfs_path.value, "/shaders/spv/frag.spv");

  const auto *combo = manifest.find_slang_shader("combo");
  ASSERT_NE(combo, nullptr);
  EXPECT_EQ(combo->mode, Config::SlangOutputMode::Both);
  EXPECT_EQ(combo->ir_vfs_path.value, "/shaders/slang/combo.slang-module");
  EXPECT_EQ(combo->spv_vfs_path.value, "/shaders/spv/combo.spv");
}

TEST(BuilderTest, SpirvFileBuilderPipelineRegistersDiskShader)
{
  Manifest manifest;
  open(manifest) | into({"/spv"}) | from(".") | map_parallel()
      | spirv_file("prebuilt")
            .source("pipeline/frag.spv")
            .entry("main")
            .stage(Stage::Fragment)
      | build();

  const auto *shader = manifest.find_spirv_shader("prebuilt");
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->entry_point, "main");
  EXPECT_EQ(shader->stage, Stage::Fragment);
  EXPECT_EQ(shader->vfs_path.value, "/spv/pipeline/frag.spv");
}

TEST(BuilderTest, SpirvFileAtOverrideUsesExactVirtualPath)
{
  Manifest manifest;
  open(manifest)
      | spirv_file("legacy")
            .source("old/legacy.spv")
            .entry("main")
            .stage(Stage::Vertex)
            .at({"/compat/legacy.spv"})
      | build();

  const auto *shader = manifest.find_spirv_shader("legacy");
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->vfs_path.value, "/compat/legacy.spv");
}

TEST(BuilderTest, ModuleIrAtOverrideUsesExactVirtualPath)
{
  Manifest manifest;
  open(manifest)
      | module("math")
            .sources({"math.slang"})
            .import_as("math_utils")
            .ir_at({"/custom/math.ir"})
      | build();

  const auto *mod = manifest.find_slang_module("math");
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(mod->ir_vfs_path.value, "/custom/math.ir");
}

TEST(BuilderTest, SlangArtifactAtOverridesUseExactVirtualPaths)
{
  Manifest manifest;
  open(manifest) | module("math").sources({"math.slang"})
      | slang("vert")
            .source("vert.slang")
            .entry("vsMain")
            .stage(Stage::Vertex)
            .both()
            .uses("math")
            .ir_at({"/custom/vert.ir"})
            .spv_at({"/custom/vert.spv"})
      | build();

  const auto *shader = manifest.find_slang_shader("vert");
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->ir_vfs_path.value, "/custom/vert.ir");
  EXPECT_EQ(shader->spv_vfs_path.value, "/custom/vert.spv");
}

TEST(BuilderTest, ParallelMappingAppliesToBuilderPipeline)
{
  Manifest manifest;
  open(manifest) | into({"/pack"}) | from(".") | map_parallel()
      | module("math").sources({"shaders/math.slang"}).import_as("math_utils")
      | slang("vert")
            .source("shaders/vert.slang")
            .entry("vsMain")
            .stage(Stage::Vertex)
            .ir()
            .uses("math")
      | build();

  const auto *mod = manifest.find_slang_module("math");
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(mod->ir_vfs_path.value, "/pack/shaders/math.slang-module");

  const auto *shader = manifest.find_slang_shader("vert");
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->ir_vfs_path.value, "/pack/shaders/vert.slang-module");
}

TEST(BuilderTest, SlangShaderRefFromImperativeCommit)
{
  Manifest manifest;
  ManifestRegister reg = open(manifest);
  (void)module(reg, "math").sources({"math.slang"}).commit(reg);

  const SlangShaderRef frag = slang(reg, "frag")
                                  .source("frag.slang")
                                  .entry("fsMain")
                                  .stage(Stage::Fragment)
                                  .spirv()
                                  .uses("math")
                                  .commit();

  EXPECT_EQ(frag.name(), "frag");
  EXPECT_EQ(frag.mode(), Config::SlangOutputMode::SpirV);
  ASSERT_NE(frag.get(), nullptr);
  EXPECT_EQ(frag.get()->spv_vfs_path.value, "/shaders/spv/frag.spv");
}

TEST(BuilderTest, SpirvShaderRefFromImperativeCommit)
{
  Manifest manifest;
  ManifestRegister reg = open(manifest);

  const SpirVShaderRef prebuilt = spirv_file(reg, "prebuilt")
                                      .source("frag.spv")
                                      .entry("main")
                                      .stage(Stage::Fragment)
                                      .at({"/spv/frag.spv"})
                                      .commit();

  EXPECT_EQ(prebuilt.name(), "prebuilt");
  ASSERT_NE(prebuilt.get(), nullptr);
  EXPECT_EQ(prebuilt.get()->vfs_path.value, "/spv/frag.spv");
}

TEST(BuilderTest, ModuleRefIrVfsPathAndStabilityAfterFurtherRegistration)
{
  Manifest manifest;
  ManifestRegister reg = open(manifest);
  const ModuleRef math = module(reg, "math")
                             .sources({"math.slang"})
                             .import_as("math_utils")
                             .commit();

  EXPECT_EQ(math.ir_vfs_path().value, "/shaders/slang/math_utils.slang-module");

  (void)slang(reg, "vert")
      .source("vert.slang")
      .entry("vsMain")
      .stage(Stage::Vertex)
      .ir()
      .uses(math)
      .commit(reg);

  EXPECT_EQ(math.name(), "math");
  EXPECT_EQ(math.import_name(), "math_utils");
  EXPECT_EQ(math.get()->module_name, "math_utils");
}

TEST(BuilderTest, UsesDeduplicatesDependencies)
{
  Manifest manifest;
  open(manifest) | module("math").sources({"math.slang"})
      | slang("vert")
            .source("vert.slang")
            .entry("vsMain")
            .stage(Stage::Vertex)
            .ir()
            .uses("math")
            .uses("math")
      | build();

  const auto *shader = manifest.find_slang_shader("vert");
  ASSERT_NE(shader, nullptr);
  ASSERT_EQ(shader->dependencies.size(), 1U);
  EXPECT_EQ(shader->dependencies.front(), "math");
}

TEST(BuilderTest, UsesMultipleModuleRefsInImperativeCommit)
{
  Manifest manifest;
  ManifestRegister reg = open(manifest);
  const ModuleRef math = module(reg, "math").sources({"math.slang"}).commit();
  const ModuleRef utils =
      module(reg, "utils").sources({"utils.slang"}).commit();

  (void)slang(reg, "vert")
      .source("vert.slang")
      .entry("vsMain")
      .stage(Stage::Vertex)
      .ir()
      .uses({math, utils})
      .commit(reg);

  const auto *shader = manifest.find_slang_shader("vert");
  ASSERT_NE(shader, nullptr);
  ASSERT_EQ(shader->dependencies.size(), 2U);
  EXPECT_EQ(shader->dependencies[0], "math");
  EXPECT_EQ(shader->dependencies[1], "utils");
}

TEST(BuilderTest, ModuleRejectsEmptySourcesInPipeline)
{
  Manifest manifest;
  EXPECT_THROW(open(manifest) | module("empty") | build(),
               std::invalid_argument);
}

TEST(BuilderTest, SlangRejectsMissingSource)
{
  Manifest manifest;
  EXPECT_THROW(open(manifest)
                   | slang("vert").entry("vsMain").stage(Stage::Vertex).ir()
                   | build(),
               std::invalid_argument);
}

TEST(BuilderTest, SlangRejectsMissingMode)
{
  Manifest manifest;
  EXPECT_THROW(open(manifest)
                   | slang("vert")
                         .source("vert.slang")
                         .entry("vsMain")
                         .stage(Stage::Vertex)
                   | build(),
               std::invalid_argument);
}

TEST(BuilderTest, SlangRejectsMissingStage)
{
  Manifest manifest;
  EXPECT_THROW(open(manifest)
                   | slang("vert").source("vert.slang").entry("vsMain").ir()
                   | build(),
               std::invalid_argument);
}

TEST(BuilderTest, SpirvFileRejectsMissingStage)
{
  Manifest manifest;
  EXPECT_THROW(open(manifest)
                   | spirv_file("prebuilt").source("frag.spv").entry("main")
                   | build(),
               std::invalid_argument);
}

TEST(BuilderTest, BuildAliasMatchesRegisterAll)
{
  Manifest manifest;
  open(manifest) | module("math").sources({"math.slang"}) | register_all();

  Manifest manifest2;
  open(manifest2) | module("math").sources({"math.slang"}) | build();

  EXPECT_EQ(manifest.num_slang_modules(), manifest2.num_slang_modules());
  ASSERT_NE(manifest.find_slang_module("math"), nullptr);
  ASSERT_NE(manifest2.find_slang_module("math"), nullptr);
  EXPECT_EQ(manifest.find_slang_module("math")->ir_vfs_path.value,
            manifest2.find_slang_module("math")->ir_vfs_path.value);
}

} // namespace
} // namespace AkRender::ShaderSetGenerator
