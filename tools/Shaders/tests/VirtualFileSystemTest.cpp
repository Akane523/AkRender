#include <AkRender/Shaders/VirtualFileSystem.hpp>

#include <gtest/gtest.h>

namespace
{

using namespace AkRender::Shaders;

// ── Flat node array — one single aggregate initialiser! ────────────────────
//
//  Index  Content                          type
//  ─────  ───────                          ────
//  0      /                        (root)  dir   children: [1, 2]
//  1      manifest.json                    file
//  2      /shaders/                 (dir)  dir   children: [3, 4, 5]
//  3      triangle.spv                     file
//  4      quad.spv                         file
//  5      /shaders/slang/          (dir)   dir   children: [6]
//  6      math_utils.slang-module          file

constexpr Node fs_nodes[] = {
    // 0: root "/"
    Node{"",             NodeType::Directory, 1, 2, {}},
    // 1
    Node{"manifest.json", NodeType::File,     0, 0, {448, 50}},
    // 2: "/shaders/"
    Node{"shaders",      NodeType::Directory, 3, 3, {}},
    // 3
    Node{"triangle.spv", NodeType::File,     0, 0, {0, 128}},
    // 4
    Node{"quad.spv",     NodeType::File,     0, 0, {128, 64}},
    // 5: "/shaders/slang/"
    Node{"slang",        NodeType::Directory, 6, 1, {}},
    // 6
    Node{"math_utils.slang-module", NodeType::File, 0, 0, {192, 256}},
};

constexpr VirtualFileSystem test_fs{fs_nodes, 7};

// ── Compile-time checks (static_assert) ─────────────────────────────────────

// Root
static_assert(test_fs.lookup("/") == &fs_nodes[0]);

// File lookups
static_assert(test_fs.lookup("/manifest.json")                == &fs_nodes[1]);
static_assert(test_fs.lookup("/shaders/triangle.spv")         == &fs_nodes[3]);
static_assert(test_fs.lookup("/shaders/quad.spv")             == &fs_nodes[4]);
static_assert(test_fs.lookup("/shaders/slang/math_utils.slang-module")
                                                              == &fs_nodes[6]);

// Directory lookups
static_assert(test_fs.lookup_directory("/")            == &fs_nodes[0]);
static_assert(test_fs.lookup_directory("/shaders")     == &fs_nodes[2]);
static_assert(test_fs.lookup_directory("/shaders/slang") == &fs_nodes[5]);

// Non-existent paths
static_assert(test_fs.lookup("/nonexistent") == nullptr);
static_assert(test_fs.lookup("/shaders/missing.spv") == nullptr);
static_assert(test_fs.lookup("/shaders/slang/missing.slang-module")
              == nullptr);
static_assert(test_fs.lookup("relative/path") == nullptr);
static_assert(test_fs.lookup_directory("/nonexistent/dir") == nullptr);

// path ending with slash returns the directory node
static_assert(test_fs.lookup("/shaders/") == &fs_nodes[2]);

// record() shorthand
static_assert(test_fs.record("/manifest.json").offset == 448);
static_assert(test_fs.record("/shaders/triangle.spv").size == 128);
static_assert(test_fs.record("/nonexistent").empty());

// ── Runtime tests ───────────────────────────────────────────────────────────

TEST(VirtualFileSystemTest, LookupFile)
{
  ASSERT_NE(test_fs.lookup("/manifest.json"), nullptr);
  EXPECT_EQ(test_fs.lookup("/manifest.json")->data.offset, 448);
  EXPECT_EQ(test_fs.lookup("/manifest.json")->data.size, 50);

  ASSERT_NE(test_fs.lookup("/shaders/triangle.spv"), nullptr);
  EXPECT_EQ(test_fs.lookup("/shaders/triangle.spv")->data.offset, 0);
  EXPECT_EQ(test_fs.lookup("/shaders/triangle.spv")->data.size, 128);
}

TEST(VirtualFileSystemTest, LookupDirectory)
{
  auto* root = test_fs.lookup_directory("/");
  ASSERT_NE(root, nullptr);
  EXPECT_TRUE(root->is_directory());

  auto* shaders = test_fs.lookup_directory("/shaders");
  ASSERT_NE(shaders, nullptr);
  EXPECT_TRUE(shaders->is_directory());
  EXPECT_EQ(shaders->num_children, 3u);
  EXPECT_EQ(shaders->first_child, 3u);

  auto* slang = test_fs.lookup_directory("/shaders/slang");
  ASSERT_NE(slang, nullptr);
  EXPECT_TRUE(slang->is_directory());
  EXPECT_EQ(slang->num_children, 1u);
}

TEST(VirtualFileSystemTest, LookupNonexistent)
{
  EXPECT_EQ(test_fs.lookup("/nonexistent"), nullptr);
  EXPECT_EQ(test_fs.lookup("/shaders/missing.spv"), nullptr);
  EXPECT_EQ(test_fs.lookup("/shaders/slang/missing.slang-module"), nullptr);
  EXPECT_EQ(test_fs.lookup("relative/path"), nullptr);
  EXPECT_EQ(test_fs.lookup_directory("/nonexistent/dir"), nullptr);
}

TEST(VirtualFileSystemTest, TrailingSlash)
{
  // lookup with trailing slash returns the directory
  auto* n = test_fs.lookup("/shaders/");
  ASSERT_NE(n, nullptr);
  EXPECT_TRUE(n->is_directory());
  EXPECT_EQ(n, &fs_nodes[2]);
}

TEST(VirtualFileSystemTest, RecordShorthand)
{
  EXPECT_EQ(test_fs.record("/manifest.json").offset, 448u);
  EXPECT_EQ(test_fs.record("/shaders/triangle.spv").size, 128u);
  EXPECT_TRUE(test_fs.record("/nonexistent").empty());
  EXPECT_TRUE(test_fs.record("/shaders").empty()); // directory -> empty
}

TEST(VirtualFileSystemTest, ChildrenIteration)
{
  auto* shaders = test_fs.lookup_directory("/shaders");
  ASSERT_NE(shaders, nullptr);

  for (std::size_t i = shaders->first_child;
       i < shaders->first_child + shaders->num_children; ++i)
  {
    bool found = false;
    for (std::size_t j = 0; j < shaders->num_children; ++j)
    {
      if (&fs_nodes[shaders->first_child + j] == &fs_nodes[i])
      {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
}

TEST(VirtualFileSystemTest, NodeTypeChecks)
{
  EXPECT_TRUE(fs_nodes[0].is_directory());
  EXPECT_TRUE(fs_nodes[1].is_file());
  EXPECT_TRUE(fs_nodes[2].is_directory());
  EXPECT_TRUE(fs_nodes[3].is_file());
}

} // anonymous namespace