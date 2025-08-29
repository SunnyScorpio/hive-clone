#include "rules.hpp"
#include <unordered_set>
#include <queue>
#include <functional>

namespace hive {

    bool occupied(const GameState& s, Axial a) {
        return s.board().find(a) != s.board().end();
    }

    int stackHeight(const GameState& s, Axial a) {
        auto it = s.board().find(a);
        return it == s.board().end() ? -1 : (int)it->second.size() - 1;
    }

    bool queenSurrounded(const GameState& s, Color c) {
        // Find the queen's current cell (if any)
        std::optional<Axial> qpos;
        for (const auto& [cell, stack] : s.board()) {
            for (int pid : stack) {
                const Piece& p = s.pieces()[pid];
                if (p.color == c && p.bug == Bug::Queen) {
                    qpos = cell;
                    break;
                }
            }
            if (qpos) break;
        }
        if (!qpos) return false; // queen not placed yet

        // Surrounded if all 6 neighbors are occupied (any color/stack)
        for (int i = 0; i < kHexDirCount; ++i) {
            Axial n = add(*qpos, dir(i));
            if (!occupied(s, n)) return false;
        }
        return true;
    }

    GameOver evaluateGameOver(const GameState& s) {
        bool w = queenSurrounded(s, Color::White);
        bool b = queenSurrounded(s, Color::Black);
        if (w && b) return GameOver::Draw;
        if (w)      return GameOver::BlackWins;
        if (b)      return GameOver::WhiteWins;
        return GameOver::None;
    }


    bool canSlideBetween(const GameState& s, Axial from, Axial to) {
        int dirIndex = -1;
        for (int i = 0; i < kHexDirCount; ++i) { if (add(from, dir(i)) == to) { dirIndex = i; break; } }
        if (dirIndex < 0) return false;
        Axial left = add(from, dir((dirIndex + 5) % 6));
        Axial right = add(from, dir((dirIndex + 1) % 6));
        bool leftBlocked = occupied(s, left);
        bool rightBlocked = occupied(s, right);
        return !(leftBlocked && rightBlocked);
    }

    bool keepsHiveConnectedAfter(const GameState& s, int movingPid, Axial to) {
        // If destination equals source, connectivity is unchanged.
        const Piece& mp = s.pieces()[movingPid];
        const Axial from = mp.pos;
        if (to.q == from.q && to.r == from.r) return true;

        // Pack (q,r) into 64 bits
        auto pack = [](Axial a)->std::int64_t {
            return (static_cast<std::int64_t>(a.q) << 32) |
                static_cast<std::int64_t>(static_cast<std::uint32_t>(a.r));
            };

        // Build the set of occupied cells AFTER the move:
        //  - remove the moving piece from 'from' (assume legal/top move elsewhere)
        //  - add it onto 'to'
        std::unordered_map<std::int64_t, int> occCount;
        occCount.reserve(s.board().size() + 1);

        for (const auto& [pos, stack] : s.board()) {
            int count = static_cast<int>(stack.size());
            // If this is the 'from' cell, remove the moving piece from its count
            if (pos.q == from.q && pos.r == from.r) {
                // Normally only the top can move; we just decrement count
                if (count > 0) --count;
            }
            if (count > 0) {
                occCount[pack(pos)] += count;
            }
        }
        // Add destination occupancy for the moved piece
        occCount[pack(to)] += 1;

        // Prune any zero-count entries (defensive)
        std::vector<std::int64_t> toErase;
        for (auto& kv : occCount) if (kv.second <= 0) toErase.push_back(kv.first);
        for (auto k : toErase) occCount.erase(k);

        // If no occupied cells remain (shouldn't really happen), treat as connected.
        if (occCount.empty()) return true;

        // BFS over occupied cells (nodes are cells with occCount>0; edges between adjacent occupied cells)
        auto unpack = [](std::int64_t k)->Axial {
            int q = static_cast<int>(k >> 32);
            int r = static_cast<int>(static_cast<std::int32_t>(k & 0xFFFFFFFFll));
            return { q, r };
            };

        std::queue<std::int64_t> q;
        std::unordered_set<std::int64_t> seen;
        seen.reserve(occCount.size() * 2);

        // start from any occupied cell
        q.push(occCount.begin()->first);
        seen.insert(occCount.begin()->first);

        while (!q.empty()) {
            auto curK = q.front(); q.pop();
            Axial cur = unpack(curK);

            for (int i = 0; i < kHexDirCount; ++i) {
                Axial n = add(cur, dir(i));
                auto nk = pack(n);
                // neighbor is "connected" if that cell is occupied (count > 0)
                auto it = occCount.find(nk);
                if (it != occCount.end() && it->second > 0) {
                    if (seen.insert(nk).second) q.push(nk);
                }
            }
        }

        // Connected iff we visited every occupied cell
        return (seen.size() == occCount.size());
    }


    static void queenMoves(const GameState& s, int pid, std::vector<LegalMove>& out) {
        const auto& p = s.pieces()[pid];
        for (int i = 0; i < kHexDirCount; ++i) {
            Axial dest = add(p.pos, dir(i));
            if (!occupied(s, dest) && canSlideBetween(s, p.pos, dest) && keepsHiveConnectedAfter(s, pid, dest)) {
                out.push_back({ pid,p.pos,dest,MoveKind::Slide,1 });
            }
        }
    }

    static void beetleMoves(const GameState& s, int pid, std::vector<LegalMove>& out) {
        const auto& p = s.pieces()[pid];
        const Axial from = p.pos;

        // Height of the stack at `from` (top index), -1 if empty (defensive)
        int hFrom = stackHeight(s, from);

        // Helper: corridor (freedom-to-move) check at ground level
        auto canSlideBetweenGround = [&](Axial a, Axial b)->bool {
            return canSlideBetween(s, a, b);
            };

        for (int i = 0; i < kHexDirCount; ++i) {
            Axial to = add(from, dir(i));
            bool destOcc = occupied(s, to);

            // If destination is occupied, Beetle may always climb there (ignores corridor rule).
            if (destOcc) {
                if (keepsHiveConnectedAfter(s, pid, to)) {
                    out.push_back({ pid, from, to, MoveKind::Climb, 1 });
                }
                continue;
            }

            // Destination is empty:
            // - If Beetle is on TOP of a stack (hFrom > 0), it can step down to any adjacent empty
            //   and it ignores the corridor rule (it’s moving along/over the hive).
            // - If Beetle is on ground (hFrom == 0), it must obey the corridor rule to slide.
            bool allowed = (hFrom > 0) ? true : canSlideBetweenGround(from, to);

            if (allowed && keepsHiveConnectedAfter(s, pid, to)) {
                // Tag as Slide when landing on empty; Climb when moving onto a piece
                out.push_back({ pid, from, to, MoveKind::Slide, 1 });
            }
        }
    }


    static void grasshopperMoves(const GameState& s, int pid, std::vector<LegalMove>& out) {
        const auto& p = s.pieces()[pid];
        for (int i = 0; i < kHexDirCount; ++i) {
            Axial cur = add(p.pos, dir(i));
            bool jumped = false;
            while (occupied(s, cur)) { jumped = true; cur = add(cur, dir(i)); }
            if (jumped && !occupied(s, cur) && keepsHiveConnectedAfter(s, pid, cur)) {
                out.push_back({ pid,p.pos,cur,MoveKind::Jump,0 });
            }
        }
    }

    static void antMoves(const GameState& s, int pid, std::vector<LegalMove>& out) {
        const auto& p = s.pieces()[pid];
        const Axial start = p.pos;

        // Treat the start hex as empty while exploring.
        auto occ = [&](Axial a)->bool {
            return !(a.q == start.q && a.r == start.r) && occupied(s, a);
            };

        // A cell is "on the hive perimeter" if it's empty and adjacent to at least one occupied cell.
        auto adjacentToHive = [&](Axial a)->bool {
            for (int i = 0; i < kHexDirCount; ++i) {
                Axial n = add(a, dir(i));
                if (occ(n)) return true;
            }
            return false;
            };

        // Slide corridor check that considers the start cell empty.
        auto canSlideBetweenConsideringStart = [&](Axial from, Axial to)->bool {
            int dirIndex = -1;
            for (int i = 0; i < kHexDirCount; ++i) { if (add(from, dir(i)) == to) { dirIndex = i; break; } }
            if (dirIndex < 0) return false;
            Axial left = add(from, dir((dirIndex + kHexDirCount - 1) % kHexDirCount));
            Axial right = add(from, dir((dirIndex + 1) % kHexDirCount));
            bool leftBlocked = occ(left);
            bool rightBlocked = occ(right);
            return !(leftBlocked && rightBlocked);
            };

        auto pack = [](Axial a)->std::int64_t {
            return (static_cast<std::int64_t>(a.q) << 32) |
                static_cast<std::int64_t>(static_cast<std::uint32_t>(a.r));
            };

        std::queue<Axial> q;
        std::unordered_set<std::int64_t> seen;

        // Seed from start’s slide-legal empty neighbors that are on the hive perimeter.
        for (int i = 0; i < kHexDirCount; ++i) {
            Axial n = add(start, dir(i));
            if (!occ(n) && adjacentToHive(n) && canSlideBetweenConsideringStart(start, n)) {
                auto key = pack(n);
                if (seen.insert(key).second) q.push(n);
            }
        }

        // BFS across empty perimeter cells only
        while (!q.empty()) {
            Axial cur = q.front(); q.pop();

            // Any visited perimeter cell is a legal destination (if hive stays connected)
            if (keepsHiveConnectedAfter(s, pid, cur)) {
                out.push_back({ pid, start, cur, MoveKind::Slide, /*steps*/0 });
            }

            for (int i = 0; i < kHexDirCount; ++i) {
                Axial nxt = add(cur, dir(i));
                if (occ(nxt)) continue;                              // cannot occupy a piece
                if (!adjacentToHive(nxt)) continue;                  // stay on the hive perimeter
                if (!canSlideBetweenConsideringStart(cur, nxt)) continue;

                auto key = pack(nxt);
                if (seen.insert(key).second) {
                    q.push(nxt);
                }
            }
        }
    }



    static void spiderMoves(const GameState& s, int pid, std::vector<LegalMove>& out) {
        const auto& piece = s.pieces()[pid];
        const Axial start = piece.pos;

        // Treat the start hex as empty while exploring
        auto occ = [&](Axial a)->bool {
            return !(a.q == start.q && a.r == start.r) && occupied(s, a);
            };

        // Perimeter check: empty cell that touches at least one occupied cell
        auto adjacentToHive = [&](Axial a)->bool {
            for (int i = 0; i < kHexDirCount; ++i) {
                if (occ(add(a, dir(i)))) return true;
            }
            return false;
            };

        // Sliding corridor check that treats the start as empty
        auto canSlideBetweenConsideringStart = [&](Axial from, Axial to)->bool {
            int dirIndex = -1;
            for (int i = 0; i < kHexDirCount; ++i) { if (add(from, dir(i)) == to) { dirIndex = i; break; } }
            if (dirIndex < 0) return false;
            Axial left = add(from, dir((dirIndex + kHexDirCount - 1) % kHexDirCount));
            Axial right = add(from, dir((dirIndex + 1) % kHexDirCount));
            bool leftBlocked = occ(left);
            bool rightBlocked = occ(right);
            return !(leftBlocked && rightBlocked);
            };

        auto pack = [](Axial a)->std::int64_t {
            return (static_cast<std::int64_t>(a.q) << 32) |
                static_cast<std::int64_t>(static_cast<std::uint32_t>(a.r));
            };

        // Depth-limited DFS (exactly 3 steps), no revisits
        std::unordered_set<std::int64_t> visited;
        visited.insert(pack(start));

        std::function<void(Axial, int)> dfs = [&](Axial cur, int depth) {
            if (depth == 3) {
                if (!(cur.q == start.q && cur.r == start.r)) {
                    if (keepsHiveConnectedAfter(s, pid, cur)) {
                        out.push_back({ pid, start, cur, MoveKind::Slide, /*steps*/3 });
                    }
                }
                return;
            }

            for (int i = 0; i < kHexDirCount; ++i) {
                Axial nxt = add(cur, dir(i));
                if (occ(nxt)) continue;                         // cannot step onto occupied
                if (!adjacentToHive(nxt)) continue;             // stay on perimeter
                if (!canSlideBetweenConsideringStart(cur, nxt)) continue;

                auto key = pack(nxt);
                if (visited.insert(key).second) {
                    dfs(nxt, depth + 1);
                    visited.erase(key);                          // backtrack
                }
            }
            };

        dfs(start, 0);
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