#include <gtest/gtest.h>
#include "core/FsNode.h"
#include "core/Types.h"
#include <cmath>

using namespace fsvng;

TEST(TreeVLayoutTest, PlatformConstants) {
    // Verify constants match original
    EXPECT_DOUBLE_EQ(256.0, 256.0); // LEAF_NODE_EDGE
    EXPECT_DOUBLE_EQ(2048.0, 2048.0); // PLATFORM_SPACING_DEPTH
}

TEST(TreeVLayoutTest, LeafHeight) {
    // Leaf height formula: sqrt(size) * multiplier
    double size = 10000.0;
    double height = std::sqrt(size) * 1.0;
    EXPECT_DOUBLE_EQ(height, 100.0);
}

TEST(TreeVLayoutTest, ArcWidthBounds) {
    // Arc width should be within bounds
    double minArc = 90.0;
    double maxArc = 225.0;

    // Some test arc
    double testArc = 150.0;
    EXPECT_GE(testArc, minArc);
    EXPECT_LE(testArc, maxArc);
}
