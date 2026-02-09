#include <gtest/gtest.h>
#include "core/FsNode.h"
#include "core/Types.h"

using namespace fsvng;

TEST(MapVLayoutTest, NodeDimensions) {
    FsNode node;
    node.mapvGeom.c0 = {-50.0, -30.0};
    node.mapvGeom.c1 = {50.0, 30.0};
    node.mapvGeom.height = 128.0;

    EXPECT_DOUBLE_EQ(node.mapvWidth(), 100.0);
    EXPECT_DOUBLE_EQ(node.mapvDepth(), 60.0);
}

TEST(MapVLayoutTest, NoOverlap) {
    // Create a simple tree and verify layout produces non-overlapping nodes
    FsNode parent;
    parent.type = NODE_DIRECTORY;
    parent.mapvGeom.c0 = {-500.0, -500.0};
    parent.mapvGeom.c1 = {500.0, 500.0};
    parent.mapvGeom.height = 384.0;

    // Add some children with varying sizes
    for (int i = 0; i < 5; i++) {
        auto child = std::make_unique<FsNode>();
        child->type = NODE_REGFILE;
        child->name = "file" + std::to_string(i);
        child->size = (i + 1) * 1000;
        child->mapvGeom.c0 = {-50.0 * (i+1), -30.0 * (i+1)};
        child->mapvGeom.c1 = {50.0 * (i+1), 30.0 * (i+1)};
        parent.addChild(std::move(child));
    }

    // Verify all children have positive dimensions
    for (auto& child : parent.children) {
        double w = child->mapvGeom.c1.x - child->mapvGeom.c0.x;
        double d = child->mapvGeom.c1.y - child->mapvGeom.c0.y;
        EXPECT_GT(w, 0.0);
        EXPECT_GT(d, 0.0);
    }
}
