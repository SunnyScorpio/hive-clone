#include <gtest/gtest.h>
#include "rules.hpp"
using namespace hive;


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