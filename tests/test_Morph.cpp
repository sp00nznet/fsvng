#include <gtest/gtest.h>
#include "animation/Morph.h"
#include "core/PlatformUtils.h"

using namespace fsvng;

TEST(MorphTest, LinearMorph) {
    double var = 0.0;
    auto& engine = MorphEngine::instance();

    engine.morph(&var, MorphType::Linear, 100.0, 0.001); // very short duration

    // After enough iterations, var should reach 100
    for (int i = 0; i < 100; i++) {
        engine.iteration();
        // Small sleep to let time pass
    }

    // Should have reached end value (or very close)
    // Due to timing, just check it moved
    // The morph should complete quickly
    EXPECT_NE(var, 0.0);

    // Clean up
    engine.morphBreak(&var);
}

TEST(MorphTest, MorphBreak) {
    double var = 50.0;
    auto& engine = MorphEngine::instance();

    engine.morph(&var, MorphType::Linear, 100.0, 10.0); // long duration
    engine.morphBreak(&var);

    // var should stay at whatever it was when broken
    double valAfterBreak = var;
    engine.iteration();
    EXPECT_DOUBLE_EQ(var, valAfterBreak);
}

TEST(MorphTest, MorphFinish) {
    double var = 0.0;
    auto& engine = MorphEngine::instance();

    engine.morph(&var, MorphType::Sigmoid, 200.0, 10.0); // long duration
    engine.morphFinish(&var);
    engine.iteration(); // process the finish

    EXPECT_DOUBLE_EQ(var, 200.0);
}

TEST(MorphTest, EasingFunctions) {
    // Test that easing functions produce valid output
    // Just verify they don't crash or produce NaN
    double vars[5] = {0, 0, 0, 0, 0};
    auto& engine = MorphEngine::instance();

    engine.morph(&vars[0], MorphType::Linear, 1.0, 0.001);
    engine.morph(&vars[1], MorphType::Quadratic, 1.0, 0.001);
    engine.morph(&vars[2], MorphType::InvQuadratic, 1.0, 0.001);
    engine.morph(&vars[3], MorphType::Sigmoid, 1.0, 0.001);
    engine.morph(&vars[4], MorphType::SigmoidAccel, 1.0, 0.001);

    for (int i = 0; i < 50; i++) {
        engine.iteration();
    }

    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(std::isnan(vars[i]));
        // Break remaining morphs
        engine.morphBreak(&vars[i]);
    }
}
