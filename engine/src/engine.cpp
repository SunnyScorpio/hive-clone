#include "engine.hpp"
#include <stdexcept>

namespace hive {

    GameState::GameState() {}

    int GameState::addDemoPiece(Bug bug, Color color, Axial at, int height) {
        int id = static_cast<int>(pieces_.size());
        pieces_.push_back(Piece{ id, bug, color, true, at, height });
        auto& stack = board_[at];
        if (height < 0 || height > static_cast<int>(stack.size())) height = static_cast<int>(stack.size());
        stack.insert(stack.begin() + height, id);
        // rearrange heights
        for (int i = 0; i < (int)stack.size(); ++i) pieces_[stack[i]].height = i;
        return id;
    }

    void GameState::movePiece(int pieceId, Axial to, bool allowStack) {
        if (pieceId < 0 || pieceId >= (int)pieces_.size()) throw std::runtime_error("bad pieceId");
        Piece& p = pieces_[pieceId];
        if (!p.onBoard) throw std::runtime_error("piece not on board");

        // remove from old stack
        auto itOld = board_.find(p.pos);
        auto& oldStack = itOld->second;
        oldStack.erase(oldStack.begin() + p.height);
        for (int i = 0; i < (int)oldStack.size(); ++i) pieces_[oldStack[i]].height = i;
        if (oldStack.empty()) board_.erase(itOld);

        // add to new stack
        auto& newStack = board_[to];
        int newH = allowStack ? (int)newStack.size() : 0;
        newStack.insert(newStack.begin() + newH, pieceId);
        for (int i = 0; i < (int)newStack.size(); ++i) pieces_[newStack[i]].height = i;

        p.pos = to;
        p.height = newH;
    }

    Pixel axialToPixel(Axial a, float s) {
        // axial q,r -> pixel x,y for pointy-top hexes
        // x = s * (sqrt(3) * q + sqrt(3)/2 * r)
        // y = s * (3/2 * r)
        constexpr float SQ3 = 1.7320508075688772f;
        float x = s * (SQ3 * a.q + (SQ3 * 0.5f) * a.r);
        float y = s * (1.5f * a.r);
        return { x, y };
    }

} // namespace hive