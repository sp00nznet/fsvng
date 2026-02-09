#include <gtest/gtest.h>
#include "core/FsNode.h"
#include "core/FsTree.h"

using namespace fsvng;

TEST(FsNodeTest, CreateNode) {
    FsNode node;
    node.type = NODE_REGFILE;
    node.name = "test.txt";
    node.size = 1024;
    EXPECT_EQ(node.type, NODE_REGFILE);
    EXPECT_EQ(node.name, "test.txt");
    EXPECT_EQ(node.size, 1024);
    EXPECT_FALSE(node.isDir());
}

TEST(FsNodeTest, CreateDirectory) {
    FsNode node;
    node.type = NODE_DIRECTORY;
    node.name = "mydir";
    EXPECT_TRUE(node.isDir());
    EXPECT_TRUE(node.isCollapsed()); // deployment = 0
}

TEST(FsNodeTest, AddChildren) {
    FsNode parent;
    parent.type = NODE_DIRECTORY;
    parent.name = "parent";

    auto child1 = std::make_unique<FsNode>();
    child1->type = NODE_REGFILE;
    child1->name = "file1.txt";
    child1->size = 100;

    auto child2 = std::make_unique<FsNode>();
    child2->type = NODE_REGFILE;
    child2->name = "file2.txt";
    child2->size = 200;

    FsNode* c1 = parent.addChild(std::move(child1));
    FsNode* c2 = parent.addChild(std::move(child2));

    EXPECT_EQ(parent.childCount(), 2);
    EXPECT_EQ(c1->parent, &parent);
    EXPECT_EQ(c2->parent, &parent);
    EXPECT_EQ(c1->name, "file1.txt");
}

TEST(FsNodeTest, AbsName) {
    FsNode meta;
    meta.type = NODE_METANODE;
    meta.name = "/home";

    auto dir = std::make_unique<FsNode>();
    dir->type = NODE_DIRECTORY;
    dir->name = "user";

    auto file = std::make_unique<FsNode>();
    file->type = NODE_REGFILE;
    file->name = "test.txt";

    FsNode* dirPtr = meta.addChild(std::move(dir));
    FsNode* filePtr = dirPtr->addChild(std::move(file));

    std::string path = filePtr->absName();
    EXPECT_EQ(path, "/home/user/test.txt");
}

TEST(FsNodeTest, MapVHelpers) {
    FsNode node;
    node.mapvGeom.c0 = {-100.0, -50.0};
    node.mapvGeom.c1 = {100.0, 50.0};

    EXPECT_DOUBLE_EQ(node.mapvWidth(), 200.0);
    EXPECT_DOUBLE_EQ(node.mapvDepth(), 100.0);
    EXPECT_DOUBLE_EQ(node.mapvCenterX(), 0.0);
    EXPECT_DOUBLE_EQ(node.mapvCenterY(), 0.0);
}

TEST(FsTreeTest, SetupTree) {
    auto& tree = FsTree::instance();

    auto meta = std::make_unique<FsNode>();
    meta->type = NODE_METANODE;
    meta->name = "";
    meta->id = tree.allocateId();

    auto root = std::make_unique<FsNode>();
    root->type = NODE_DIRECTORY;
    root->name = "root";
    root->id = tree.allocateId();
    root->size = 4096;

    auto file1 = std::make_unique<FsNode>();
    file1->type = NODE_REGFILE;
    file1->name = "big.dat";
    file1->id = tree.allocateId();
    file1->size = 10000;

    auto file2 = std::make_unique<FsNode>();
    file2->type = NODE_REGFILE;
    file2->name = "small.dat";
    file2->id = tree.allocateId();
    file2->size = 100;

    FsNode* rootPtr = meta->addChild(std::move(root));
    rootPtr->addChild(std::move(file1));
    rootPtr->addChild(std::move(file2));

    tree.setRoot(std::move(meta));
    tree.setupTree();

    EXPECT_NE(tree.root(), nullptr);
    EXPECT_NE(tree.rootDir(), nullptr);
    EXPECT_EQ(tree.rootDir()->name, "root");

    // Subtree size should be sum of children
    EXPECT_EQ(tree.rootDir()->subtree.size, 10100);

    // Children should be sorted by size descending
    EXPECT_EQ(tree.rootDir()->children[0]->name, "big.dat");
    EXPECT_EQ(tree.rootDir()->children[1]->name, "small.dat");

    // Node lookup
    EXPECT_NE(tree.nodeById(0), nullptr);

    tree.clear();
}
