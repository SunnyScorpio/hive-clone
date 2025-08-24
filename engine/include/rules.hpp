#pragma once
#include "engine.hpp"
#include <vector>

namespace hive {

    enum class MoveKind { Place, Slide, Climb, Jump };

    struct LegalMove {
        int pieceId;
        Axial from;
        Axial to;
        MoveKind kind;
        int steps{ 1 };
    };

    std::vector<LegalMove> legalMovesForPiece(const GameState& s, int pieceId);

    // helpers
    bool keepsHiveConnectedAfter(const GameState& s, int movingPid, Axial to);
    bool canSlideBetween(const GameState& s, Axial from, Axial to);
    bool occupied(const GameState& s, Axial a);
    int  stackHeight(const GameState& s, Axial a);

}