#include <gtest/gtest.h>
#include "core/Types.h"
#include "core/PlatformUtils.h"

using namespace fsvng;

TEST(ColorSystemTest, RainbowColor) {
    // Red at x=0
    RGBcolor red = PlatformUtils::rainbowColor(0.0);
    EXPECT_NEAR(red.r, 1.0f, 0.01f);
    EXPECT_NEAR(red.g, 0.0f, 0.01f);

    // Some color at x=0.5 (should be around green)
    RGBcolor mid = PlatformUtils::rainbowColor(0.5);
    EXPECT_FALSE(std::isnan(mid.r));
    EXPECT_FALSE(std::isnan(mid.g));
    EXPECT_FALSE(std::isnan(mid.b));

    // All values in range
    for (double x = 0.0; x <= 1.0; x += 0.1) {
        RGBcolor c = PlatformUtils::rainbowColor(x);
        EXPECT_GE(c.r, 0.0f);
        EXPECT_LE(c.r, 1.0f);
        EXPECT_GE(c.g, 0.0f);
        EXPECT_LE(c.g, 1.0f);
        EXPECT_GE(c.b, 0.0f);
        EXPECT_LE(c.b, 1.0f);
    }
}

TEST(ColorSystemTest, HeatColor) {
    // Cold at x=0
    RGBcolor cold = PlatformUtils::heatColor(0.0);
    EXPECT_NEAR(cold.r, 0.0f, 0.01f);
    EXPECT_NEAR(cold.g, 0.0f, 0.01f);
    EXPECT_NEAR(cold.b, 0.0f, 0.01f);

    // Hot at x=1
    RGBcolor hot = PlatformUtils::heatColor(1.0);
    EXPECT_NEAR(hot.r, 1.0f, 0.01f);
    EXPECT_NEAR(hot.g, 1.0f, 0.01f);
    EXPECT_NEAR(hot.b, 1.0f, 0.01f);
}

TEST(ColorSystemTest, Hex2Rgb) {
    RGBcolor c = PlatformUtils::hex2rgb("#FF0000");
    EXPECT_NEAR(c.r, 1.0f, 0.01f);
    EXPECT_NEAR(c.g, 0.0f, 0.01f);
    EXPECT_NEAR(c.b, 0.0f, 0.01f);

    c = PlatformUtils::hex2rgb("#00FF00");
    EXPECT_NEAR(c.g, 1.0f, 0.01f);

    c = PlatformUtils::hex2rgb("#A0A0A0");
    EXPECT_NEAR(c.r, 160.0f/255.0f, 0.01f);
}

TEST(ColorSystemTest, Rgb2Hex) {
    RGBcolor c{1.0f, 0.0f, 0.0f};
    std::string hex = PlatformUtils::rgb2hex(c);
    EXPECT_EQ(hex, "#FF0000");

    c = {0.0f, 1.0f, 0.0f};
    hex = PlatformUtils::rgb2hex(c);
    EXPECT_EQ(hex, "#00FF00");
}

TEST(ColorSystemTest, WildcardMatch) {
    EXPECT_TRUE(PlatformUtils::wildcardMatch("*.txt", "file.txt"));
    EXPECT_TRUE(PlatformUtils::wildcardMatch("*.txt", "path/file.txt"));
    EXPECT_FALSE(PlatformUtils::wildcardMatch("*.txt", "file.dat"));
    EXPECT_TRUE(PlatformUtils::wildcardMatch("file*", "filename.txt"));
    EXPECT_TRUE(PlatformUtils::wildcardMatch("f?le.txt", "file.txt"));
    EXPECT_FALSE(PlatformUtils::wildcardMatch("f?le.txt", "fiile.txt"));
    EXPECT_TRUE(PlatformUtils::wildcardMatch("*", "anything"));
}
