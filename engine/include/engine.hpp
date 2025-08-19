#pragma once
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <optional>

namespace hive {

    enum class Color { White, Black };
    enum class Bug { Queen, Beetle, Spider, Grasshopper, Ant };

    struct Axial {
        int q{}; int r{};
        friend bool operator==(const Axial& a, const Axial& b) { return a.q == b.q && a.r == b.r; }
    };

    struct AxialHash {
        size_t operator()(const Axial& a) const noexcept {
            // 64-bit mix (good enough for small game state maps)
            return (static_cast<uint64_t>(static_cast<uint32_t>(a.q)) * 0x9e3779b97f4a7c15ULL) ^
                (static_cast<uint64_t>(static_cast<uint32_t>(a.r)) + 0x9e3779b97f4a7c15ULL);
        }
    };

    inline Axial add(const Axial& a, const Axial& b) { return { a.q + b.q, a.r + b.r }; }
    inline Axial dir(int i) {
        static constexpr Axial D[6] = { {1,0},{1,-1},{0,-1},{-1,0},{-1,1},{0,1} };
        return D[i % 6];
    }

    struct Piece {
        int id{}; Bug bug{}; Color color{};
        bool onBoard{ false };
        Axial pos{}; // valid iff onBoard
        int height{ 0 }; // 0 = ground
    };

    struct Move {
        int pieceId{}; Axial to{}; bool isPlacement{ false };
    };

    class GameState {
    public:
        GameState();

        // minimal API for the desktop viewer
        const std::vector<Piece>& pieces() const { return pieces_; }
        const std::unordered_map<Axial, std::vector<int>, AxialHash>& board() const { return board_; }

        // demo helpers
        int addDemoPiece(Bug bug, Color color, Axial at, int height = 0);
        void movePiece(int pieceId, Axial to, bool allowStack = true);

    private:
        std::unordered_map<Axial, std::vector<int>, AxialHash> board_;
        std::vector<Piece> pieces_;
    };

    // Geometry helpers for rendering
    struct Pixel { float x{}, y{}; };
    Pixel axialToPixel(Axial a, float hexSize);

} // namespace hive