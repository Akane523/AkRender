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
    Node{"",               ChildRange{1, 2}},
    // 1
    Node{"manifest.json",  Record{448, 50}},
    // 2: "/shaders/"
    Node{"shaders",        ChildRange{3, 3}},
    // 3
    Node{"triangle.spv",   Record{0, 128}},
    // 4
    Node{"quad.spv",       Record{128, 64}},
    // 5: "/shaders/slang/"
    Node{"slang",          ChildRange{6, 1}},
    // 6
    Node{"math_utils.slang-module", Record{192, 256}},
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
  EXPECT_EQ(test_fs.lookup("/manifest.json")->data().offset, 448);
  EXPECT_EQ(test_fs.lookup("/manifest.json")->data().size, 50);

  ASSERT_NE(test_fs.lookup("/shaders/triangle.spv"), nullptr);
  EXPECT_EQ(test_fs.lookup("/shaders/triangle.spv")->data().offset, 0);
  EXPECT_EQ(test_fs.lookup("/shaders/triangle.spv")->data().size, 128);
}

TEST(VirtualFileSystemTest, LookupDirectory)
{
  auto* root = test_fs.lookup_directory("/");
  ASSERT_NE(root, nullptr);
  EXPECT_TRUE(root->is_directory());

  auto* shaders = test_fs.lookup_directory("/shaders");
  ASSERT_NE(shaders, nullptr);
  EXPECT_TRUE(shaders->is_directory());
  EXPECT_EQ(shaders->children().num_children, 3u);
  EXPECT_EQ(shaders->children().first_child, 3u);

  auto* slang = test_fs.lookup_directory("/shaders/slang");
  ASSERT_NE(slang, nullptr);
  EXPECT_TRUE(slang->is_directory());
  EXPECT_EQ(slang->children().num_children, 1u);
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

  auto& sh_children = shaders->children();
  for (std::size_t i = sh_children.first_child;
       i < sh_children.first_child + sh_children.num_children; ++i)
  {
    bool found = false;
    for (std::size_t j = 0; j < sh_children.num_children; ++j)
    {
      if (&fs_nodes[sh_children.first_child + j] == &fs_nodes[i])
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

// ── VirtualFileSystemView tests ─────────────────────────────────────────

TEST(VirtualFileSystemTest, ViewRead)
{
  constexpr std::byte fake_blob[] = {
      std::byte{0xAB},  // offset 0: triangle.spv starts here
      std::byte{0xCD},
      std::byte{0xEF},
      std::byte{0x00},
      std::byte{0x11},  // offset 128: quad.spv starts here
      std::byte{0x22},
  };

  // Create a view with a tiny fake blob.
  // triangle.spv -> Record{0, 128} – exceeds fake_blob → read returns empty
  // quad.spv     -> Record{128, 64} – offset exceeds fake_blob → empty
  const VirtualFileSystemView view{test_fs, fake_blob};

  // File that fits within blob
  auto rec = test_fs.record("/manifest.json");
  ASSERT_FALSE(rec.empty());
  EXPECT_EQ(rec.offset, 448u);
  EXPECT_EQ(rec.size, 50u);

  // read() for an existing file whose Record is out-of-bounds → empty
  auto tri = view.read("/shaders/triangle.spv");
  EXPECT_TRUE(tri.empty());

  // read() for nonexistent path
  auto none = view.read("/nonexistent");
  EXPECT_TRUE(none.empty());

  // Delegate methods still work
  EXPECT_EQ(view.lookup("/"), &fs_nodes[0]);
  EXPECT_TRUE(view.lookup_directory("/shaders")->is_directory());
  EXPECT_EQ(view.record("/shaders/quad.spv").offset, 128u);
}

} // anonymous namespace