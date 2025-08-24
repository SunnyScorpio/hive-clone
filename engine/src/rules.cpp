#include "rules.hpp"
#include <unordered_set>
#include <queue>

namespace hive {

    bool occupied(const GameState& s, Axial a) {
        return s.board().find(a) != s.board().end();
    }

    int stackHeight(const GameState& s, Axial a) {
        auto it = s.board().find(a);
        return it == s.board().end() ? -1 : (int)it->second.size() - 1;
    }

    bool canSlideBetween(const GameState& s, Axial from, Axial to) {
        int dirIndex = -1;
        for (int i = 0; i < 6; ++i) { if (add(from, dir(i)) == to) { dirIndex = i; break; } }
        if (dirIndex < 0) return false;
        Axial left = add(from, dir((dirIndex + 5) % 6));
        Axial right = add(from, dir((dirIndex + 1) % 6));
        bool leftBlocked = occupied(s, left);
        bool rightBlocked = occupied(s, right);
        return !(leftBlocked && rightBlocked);
    }

    bool keepsHiveConnectedAfter(const GameState& s, int movingPid, Axial to) {
        // TODO: Implement proper BFS to check hive connectivity
        return true; // simplified placeholder
    }

    static void queenMoves(const GameState& s, int pid, std::vector<LegalMove>& out) {
        const auto& p = s.pieces()[pid];
        for (int i = 0; i < 6; ++i) {
            Axial dest = add(p.pos, dir(i));
            if (!occupied(s, dest) && canSlideBetween(s, p.pos, dest) && keepsHiveConnectedAfter(s, pid, dest)) {
                out.push_back({ pid,p.pos,dest,MoveKind::Slide,1 });
            }
        }
    }

    static void beetleMoves(const GameState& s, int pid, std::vector<LegalMove>& out) {
        const auto& p = s.pieces()[pid];
        for (int i = 0; i < 6; ++i) {
            Axial dest = add(p.pos, dir(i));
            if (keepsHiveConnectedAfter(s, pid, dest)) {
                out.push_back({ pid,p.pos,dest, occupied(s,dest) ? MoveKind::Climb : MoveKind::Slide,1 });
            }
        }
    }

    static void grasshopperMoves(const GameState& s, int pid, std::vector<LegalMove>& out) {
        const auto& p = s.pieces()[pid];
        for (int i = 0; i < 6; ++i) {
            Axial cur = add(p.pos, dir(i));
            bool jumped = false;
            while (occupied(s, cur)) { jumped = true; cur = add(cur, dir(i)); }
            if (jumped && !occupied(s, cur) && keepsHiveConnectedAfter(s, pid, cur)) {
                out.push_back({ pid,p.pos,cur,MoveKind::Jump,0 });
            }
        }
    }

    static void antMoves(const GameState& s, int pid, std::vector<LegalMove>& out) {
        // TODO: implement BFS for sliding any distance
    }

    static void spiderMoves(const GameState& s, int pid, std::vector<LegalMove>& out) {
        // TODO: implement DFS for exactly 3 steps, no revisits
    }

    std::vector<LegalMove> legalMovesForPiece(const GameState& s, int pid) {
        std::vector<LegalMove> out;
        switch (s.pieces()[pid].bug) {
        case Bug::Queen: queenMoves(s, pid, out); break;
        case Bug::Beetle: beetleMoves(s, pid, out); break;
        case Bug::Grasshopper: grasshopperMoves(s, pid, out); break;
        case Bug::Ant: antMoves(s, pid, out); break;
        case Bug::Spider: spiderMoves(s, pid, out); break;
        }
        return out;
    }

} // namespace hive