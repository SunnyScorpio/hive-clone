#include <gtest/gtest.h>
#include "engine.hpp"
using namespace hive;

TEST(Axial, PixelMappingDeterministic) {
    auto p1 = axialToPixel({ 0,0 }, 40.0f);
    auto p2 = axialToPixel({ 1,0 }, 40.0f);
    EXPECT_NEAR(p2.x - p1.x, 69.282f, 0.01);
    EXPECT_NEAR(p2.y - p1.y, 0.0f, 0.01);
}

TEST(GameState, AddAndStack) {
    GameState s;
    int q = s.addDemoPiece(Bug::Queen, Color::White, { 0,0 });
    int b = s.addDemoPiece(Bug::Beetle, Color::Black, { 0,0 }, 1);
    ASSERT_EQ(s.board().at({ 0,0 }).size(), 2u);
    (void)q; (void)b;
}