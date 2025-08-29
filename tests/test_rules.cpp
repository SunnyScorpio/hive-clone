#include <gtest/gtest.h>
#include "rules.hpp"
#include <unordered_set>
#include <engine.hpp>

using namespace hive;

static std::int64_t packQR(Axial a) {
    return (static_cast<std::int64_t>(a.q) << 32) |
        static_cast<std::int64_t>(static_cast<std::uint32_t>(a.r));
}

static bool anyNeighborOccupied(const GameState& s, Axial a) {
    for (int i = 0; i < kHexDirCount; ++i) {
        if (occupied(s, add(a, dir(i)))) return true;
    }
    return false;
}

TEST(Rules, QueenMovesOneStep) {
	GameState s;
	int q = s.addDemoPiece(Bug::Queen, Color::White, { 0,0 });
	auto moves = legalMovesForPiece(s, q);
	EXPECT_GE(moves.size(), 1u);
}


TEST(Rules, GrasshopperJump) {
	GameState s;
	int g = s.addDemoPiece(Bug::Grasshopper, Color::White, { 0,0 });
	s.addDemoPiece(Bug::Ant, Color::Black, { 1,0 });
	s.addDemoPiece(Bug::Ant, Color::Black, { 2,-1 });
	auto moves = legalMovesForPiece(s, g);
	bool found = false;
	for (auto& m : moves) { if (m.kind == MoveKind::Jump) found = true; }
	EXPECT_TRUE(found);
}

TEST(Rules, AntSlidesAnyDistance) {
    GameState s;
    // Simple hive with some tiles to slide around
    int a = s.addDemoPiece(Bug::Ant, Color::White, { 0,0 });
    s.addDemoPiece(Bug::Queen, Color::White, { 1,0 });
    s.addDemoPiece(Bug::Grasshopper, Color::Black, { 0,1 });
    s.addDemoPiece(Bug::Spider, Color::Black, { -1,1 });

    auto moves = legalMovesForPiece(s, a);
    // Ant should have multiple slide destinations in this small setup
    EXPECT_GE(moves.size(), 4u);

    // Verify none of the destinations are occupied
    for (auto& m : moves) {
        auto it = s.board().find(m.to);
        EXPECT_TRUE(it == s.board().end()); // Ant cannot move onto occupied cells
        EXPECT_EQ(m.kind, MoveKind::Slide);
    }
}

TEST(Rules, AntPerimeterBounded) {
    GameState s;

    // Simple hive: white queen + a few blockers so the ant has places to slide,
    // but shouldn't explode over the infinite plane.
    int ant = s.addDemoPiece(Bug::Ant, Color::White, { 0,0 });
    s.addDemoPiece(Bug::Queen, Color::White, { 1,0 });
    s.addDemoPiece(Bug::Grasshopper, Color::Black, { 0,1 });
    s.addDemoPiece(Bug::Spider, Color::Black, { -1,1 });
    s.addDemoPiece(Bug::Spider, Color::White, { 1,-1 }); // some extra structure

    auto moves = legalMovesForPiece(s, ant);

    // Should produce a reasonable, finite set (bounded by perimeter).
    ASSERT_FALSE(moves.empty());
    EXPECT_LT(moves.size(), 300u) << "Ant move set seems unbounded; BFS likely leaking off-hive.";

    // All destinations must be unique, empty, slide moves, and on the hive perimeter.
    std::unordered_set<std::int64_t> seen;
    for (const auto& m : moves) {
        // unique
        EXPECT_TRUE(seen.insert(packQR(m.to)).second);

        // not occupied
        auto it = s.board().find(m.to);
        EXPECT_TRUE(it == s.board().end()) << "Ant destination is occupied";

        // kind is slide
        EXPECT_EQ(m.kind, MoveKind::Slide);

        // on hive perimeter: at least one neighbor occupied
        EXPECT_TRUE(anyNeighborOccupied(s, m.to)) << "Destination not adjacent to hive";
    }
}

TEST(Rules, SpiderExactlyThreeSteps) {
    GameState s;

    // Small hive structure
    int sp = s.addDemoPiece(Bug::Spider, Color::White, { 0,0 });
    s.addDemoPiece(Bug::Queen, Color::White, { 1,0 });
    s.addDemoPiece(Bug::Ant, Color::Black, { 0,1 });
    s.addDemoPiece(Bug::Grasshopper, Color::Black, { -1,1 });
    s.addDemoPiece(Bug::Ant, Color::White, { 1,-1 });

    auto moves = legalMovesForPiece(s, sp);

    ASSERT_FALSE(moves.empty()) << "Spider should have some 3-step slide paths.";
    for (const auto& m : moves) {
        EXPECT_EQ(m.kind, MoveKind::Slide);
        EXPECT_EQ(m.steps, 3);
        // must land on empty
        auto it = s.board().find(m.to);
        EXPECT_TRUE(it == s.board().end());
    }

    // Ensure destinations are distinct
    std::unordered_set<std::int64_t> uniq;
    auto pack = [](Axial a)->std::int64_t {
        return (static_cast<std::int64_t>(a.q) << 32) |
            static_cast<std::int64_t>(static_cast<std::uint32_t>(a.r));
        };
    for (auto& m : moves) {
        EXPECT_TRUE(uniq.insert(pack(m.to)).second);
    }
}

TEST(Rules, BeetleClimbsOntoOccupiedNeighbor) {
    GameState s;
    int b = s.addDemoPiece(Bug::Beetle, Color::White, { 0,0 });
    s.addDemoPiece(Bug::Queen, Color::Black, { 1,0 }); // occupied neighbor

    auto moves = legalMovesForPiece(s, b);
    bool canClimb = false;
    for (auto& m : moves) {
        if (m.to.q == 1 && m.to.r == 0 && m.kind == MoveKind::Climb) {
            canClimb = true; break;
        }
    }
    EXPECT_TRUE(canClimb) << "Beetle should be able to climb onto an occupied adjacent hex.";
}

TEST(Rules, BeetleIgnoresCorridorWhenOnTop) {
    GameState s;
    // Create a stack at {0,0}: Queen (bottom) then Beetle (top)
    s.addDemoPiece(Bug::Queen, Color::White, { 0,0 });
    int b = s.addDemoPiece(Bug::Beetle, Color::White, { 0,0 }); // now on top

    // Block the corridor from {0,0} -> {1,0} by occupying the two side neighbors:
    // for dir(0) = (1,0), its "left" and "right" from {0,0} are {0,1} and {1,-1}
    s.addDemoPiece(Bug::Ant, Color::Black, { 0,1 });
    s.addDemoPiece(Bug::Ant, Color::Black, { 1,-1 });

    // Destination (1,0) is empty and corridor is blocked at ground level.
    // Since Beetle is on TOP, it should still be allowed to move to (1,0).
    auto moves = legalMovesForPiece(s, b);
    bool found = false;
    for (auto& m : moves) {
        if (m.to.q == 1 && m.to.r == 0 && m.kind == MoveKind::Slide) {
            found = true; break;
        }
    }
    EXPECT_TRUE(found) << "Beetle on top should ignore corridor rule and step down to empty neighbor.";
}


