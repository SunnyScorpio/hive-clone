#include "ui_app.hpp"
#include <cmath>
#include <numbers>
#include <algorithm>
#include <unordered_set>
#include <array>
#include <string>

using namespace hive;

// ===== constants =====
constexpr float ALPHA_FADE_OFF_TURN = 0.6f;
constexpr float OVERLAY_Q_BY4_SEC = 2.0f;  // queen-by-4th overlay duration
constexpr float OVERLAY_MOVE_BEFORE_Q_SEC = 2.0f;  // move-before-queen overlay duration
constexpr float kRate = 0.20f; // smoothing factor per frame
// ===== helpers =====
sf::ConvexShape UIApp::makeHex(float size) {
    sf::ConvexShape hex; hex.setPointCount(6);
    for (int i = 0; i < 6; ++i) {
        float angle = static_cast<float>(std::numbers::pi) / 180.0f * (60.0f * static_cast<float>(i) - 30.0f);
        hex.setPoint(i, sf::Vector2f(
            static_cast<float>(std::cos(angle) * size),
            static_cast<float>(std::sin(angle) * size))
        );
    }
    hex.setOutlineThickness(3.0f);
    hex.setOutlineColor(sf::Color::Black);
    hex.setFillColor(sf::Color(235, 235, 235));
    return hex;
}

static Axial cubeToAxial(float x, float z) { return { (int)std::round(x), (int)std::round(z) }; }

Axial UIApp::pixelToAxial(sf::Vector2f p, float s) {
    constexpr float SQ3 = 1.7320508075688772f;
    float q = (p.x * (1.0f / SQ3) / s) - (p.y * (1.0f / 3.0f) / s);
    float r = (2.0f / 3.0f) * (p.y / s);
    // cube round
    float x = q, z = r, y = -x - z;
    float rx = static_cast<float>(std::round(x));
    float ry = static_cast<float>(std::round(y));
    float rz = static_cast<float>(std::round(z));
    float x_diff = std::fabs(rx - x);
    float y_diff = std::fabs(ry - y);
    float z_diff = std::fabs(rz - z);
    if (x_diff > y_diff && x_diff > z_diff) rx = -ry - rz;
    else if (y_diff > z_diff) ry = -rx - rz;
    else rz = -rx - ry;
    return cubeToAxial(rx, rz);
}

// ===== lifecycle =====
UIApp::UIApp() : window_(sf::VideoMode(1024, 768), "Hive (Desktop) – Kickstart", sf::Style::Default, sf::ContextSettings(0u, 0u, 8u)) {
    window_.setFramerateLimit(60);

    fontOk_ = font_.loadFromFile("assets/DejaVuSans.ttf");
    offset_ = sf::Vector2f(512.f, 384.f);

    // start with White
    currentTurn_ = hive::Color::White;

    // initialize unplaced piece reserves based on base Hive counts minus current board
    initReservesFromBoard();
}

void UIApp::run() {
    while (window_.isOpen()) {
        handleEvents();
        update();
        render();
    }
}

void UIApp::handleEvents() {
    sf::Event e;
    while (window_.pollEvent(e)) {
        if (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Escape) {
            pendingPlace_.reset();
            selectedPid_ = -1;
            legalTargets_.clear();
        }
        if (e.type == sf::Event::Closed) window_.close();
        if (e.type == sf::Event::MouseLeft) { hoverAx_.reset(); }
        if (e.type == sf::Event::LostFocus) { hoverAx_.reset(); }

        if (e.type == sf::Event::MouseWheelScrolled) {
            float dz = e.mouseWheelScroll.delta;
            hexSize_ = std::max(10.0f, std::min(120.0f, hexSize_ + dz * 5.0f));
        }
        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Right) {
            dragging_ = true;
            lastMouse_ = sf::Mouse::getPosition(window_);
        }
        if (e.type == sf::Event::MouseButtonReleased && e.mouseButton.button == sf::Mouse::Right) {
            dragging_ = false;
        }
        if (e.type == sf::Event::MouseMoved && dragging_) {
            auto cur = sf::Mouse::getPosition(window_);
            offset_ += sf::Vector2f(float(cur.x - lastMouse_.x), float(cur.y - lastMouse_.y));
            lastMouse_ = cur;
        }

        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
            // First, check if click is on the side tray (screen-space hit test)
            sf::Vector2i mp = sf::Mouse::getPosition(window_);
            sf::Vector2f screenPt(static_cast<float>(mp.x), static_cast<float>(mp.y));
            hive::Color hitColor; hive::Bug hitBug;
            // turn restriction: only arm pieces for currentTurn_
            if (hitTestTray(screenPt, hitColor, hitBug)) {
                
                if (hitColor == currentTurn_) {
                    // arm pending placement if we have remaining pieces of that kind
                    auto& rem = (hitColor == hive::Color::White) ? remainingWhite_ : remainingBlack_;
                    if (!queenPlaced(hitColor) && placementsMade(hitColor) >= 3 && hitBug != hive::Bug::Queen) {
                        // trigger warning banner
                        queenWarningTimer_ = OVERLAY_Q_BY4_SEC;
                    }
                    else if (rem[hitBug] > 0) {
                        pendingPlace_ = std::make_pair(hitColor, hitBug);
                        legalTargets_ = computePlacementTargets(hitColor);
                    }
				}
				else {
					// clicked on opponent's piece in tray: ignore
				}
                // stop processing this click
                continue;
            }

            // Compute axial under the cursor at click time (works for empty cells too)
            sf::Vector2f world = sf::Vector2f(static_cast<float>(mp.x), static_cast<float>(mp.y)) - offset_;
            Axial clickAx = pixelToAxial(world, hexSize_);

            if (pendingPlace_) {
                // If a tray piece is armed, only allow placement onto a legal target
                auto isTarget = std::find_if(legalTargets_.begin(), legalTargets_.end(),
                    [&](const Axial& a) { return a.q == clickAx.q && a.r == clickAx.r; }) != legalTargets_.end();

                if (isTarget) {
                    auto [pc, pb] = *pendingPlace_;
                    state_.addDemoPiece(pb, pc, clickAx);
                    auto& rem = (pc == hive::Color::White) ? remainingWhite_ : remainingBlack_;
                    if (rem[pb] > 0) rem[pb] -= 1;
                    pendingPlace_.reset();
                    legalTargets_.clear(); // rings will fade out via animation
                    // switch turn after a successful placement
                    nextTurn();
                }
                // If not a legal target, ignore (keep pending)
                continue;
            }

            if (selectedPid_ >= 0) {
                // Deselect if clicking the same piece
                auto it = state_.board().find(clickAx);
                if (it != state_.board().end() && !it->second.empty() && it->second.back() == selectedPid_) {
                    selectedPid_ = -1;
                    legalTargets_.clear();
                }
                else {
                    // Move only to a legal (teal-ring) target — supports empty cells too
                    auto isTarget = std::find_if(legalTargets_.begin(), legalTargets_.end(), [&](const Axial& a) { return a.q == clickAx.q && a.r == clickAx.r; }) != legalTargets_.end();
                    if (isTarget) {
                        // Block moving until the current player's queen is placed
                        if (!queenPlaced(currentTurn_)) {
                            moveBeforeQueenTimer_ = OVERLAY_MOVE_BEFORE_Q_SEC;   // show message
                            // clear legal targets so rings fade out:
                            legalTargets_.clear();
							//reset selection too
                            selectedPid_ = -1;
                        }
                        else {
                            state_.movePiece(selectedPid_, clickAx, /*allowStack=*/true);
                            selectedPid_ = -1;
                            legalTargets_.clear();
                            // switch turn after a successful move
                            nextTurn();
                        }
                    }
                }
            }
            else {
                // No selection yet: select top piece at click location, if any
                auto it = state_.board().find(clickAx);
                if (it != state_.board().end() && !it->second.empty()) {
                    int topPid = it->second.back();
                    const Piece& top = state_.pieces()[topPid];
                    if (top.color == currentTurn_) {            // ← Enforce turn on selection
                        selectedPid_ = topPid;
                        legalTargets_.clear();
                        for (const auto& mv : legalMovesForPiece(state_, selectedPid_)) {
                            legalTargets_.push_back(mv.to);
                        }
                    }
                }
            }
        }
    }
}

void UIApp::update() {
    if (!window_.hasFocus()) { hoverAx_.reset(); return; }

    // Mouse in window coords -> world coords
    sf::Vector2i m = sf::Mouse::getPosition(window_);
    sf::Vector2f world = sf::Vector2f(static_cast<float>(m.x), static_cast<float>(m.y)) - offset_;

    // Find nearest existing board cell; only hover if close enough
    std::optional<Axial> best;
    float bestDist2 = (hexSize_ * 0.85f) * (hexSize_ * 0.85f); // radius threshold^2

    for (const auto& [pos, stack] : state_.board()) {
        Pixel p = axialToPixel(pos, hexSize_);
        float dx = world.x - static_cast<float>(p.x);
        float dy = world.y - static_cast<float>(p.y);
        float d2 = dx * dx + dy * dy;
        if (d2 < bestDist2) {
            bestDist2 = d2;
            best = pos;
        }
    }
    hoverAx_ = best; // clears when not near any tile

    // --- Animate teal ring alphas (appear/disappear) ---
    std::unordered_set<std::int64_t> current;
    current.reserve(legalTargets_.size());
    for (const auto& a : legalTargets_) current.insert(ringKey(a));

    // Fade in current targets toward 1.0
    for (std::int64_t k : current) {
        float& alpha = ringAlpha_[k];
        alpha += (1.0f - alpha) * kRate;
        if (alpha > 1.0f) alpha = 1.0f;
    }

    // Fade out obsolete
    std::vector<std::int64_t> toErase;
    toErase.reserve(ringAlpha_.size());
    for (auto& kv : ringAlpha_) {
        if (current.find(kv.first) == current.end()) {
            kv.second += (0.0f - kv.second) * kRate;
            if (kv.second < 0.02f) toErase.push_back(kv.first);
        }
    }
    for (std::int64_t k : toErase) ringAlpha_.erase(k);

    // --- Animate white grid neighbor ring (fade in/out like teal rings) ---
    {
        // 1) Compute current neighbor-empties ("bright set")
        std::unordered_set<std::int64_t> bright;
        if (!state_.board().empty()) {
            bright.reserve(state_.board().size() * 3);
            for (const auto& [pos, stack] : state_.board()) {
                for (int i = 0; i < 6; ++i) {
                    Axial n = add(pos, dir(i));
                    if (!occupied(state_, n)) {
                        bright.insert(ringKey(n));
                    }
                }
            }
        }

        // 2) Fade in/out alphas

        // Fade in for current bright cells
        for (std::int64_t k : bright) {
            float& a = gridRingAlpha_[k];
            a += (1.0f - a) * kRate;
            if (a > 1.0f) a = 1.0f;
        }
        // Fade out others and cull tiny
        toErase.clear();
        toErase.reserve(gridRingAlpha_.size());
        for (auto& kv : gridRingAlpha_) {
            if (bright.find(kv.first) == bright.end()) {
                kv.second += (0.0f - kv.second) * kRate;
                if (kv.second < 0.02f) toErase.push_back(kv.first);
            }
        }
        for (auto k : toErase) gridRingAlpha_.erase(k);
    }

    if (queenWarningTimer_ > 0.f) {
        queenWarningTimer_ -= 1.f / 60.f; // assuming 60 fps cap
        if (queenWarningTimer_ < 0.f) queenWarningTimer_ = 0.f;
    }
    if (moveBeforeQueenTimer_ > 0.f) {
        moveBeforeQueenTimer_ -= 1.f / 60.f; // assuming 60 fps cap
        if (moveBeforeQueenTimer_ < 0.f) moveBeforeQueenTimer_ = 0.f;
    }
}


// ===== render helpers =====

// ===== ring key helpers =====
std::int64_t UIApp::ringKey(hive::Axial a) {
    // Pack (q,r) into 64 bits: high 32 = q (signed), low 32 = r (unsigned representation)
    return ((static_cast<std::int64_t>(a.q) << 32) |
        (static_cast<std::int64_t>(static_cast<std::uint32_t>(a.r))));
}

hive::Axial UIApp::axialFromKey(std::int64_t k) {
    int q = static_cast<int>(k >> 32);
    int r = static_cast<int>(static_cast<std::int32_t>(k & 0xFFFFFFFFll));
    return { q,r };
}

void UIApp::drawBackgroundGrid(sf::RenderTarget& rt, float baseSize) {
    // cached, faint grid with AA via 2x supersample
    static sf::RenderTexture gridRT;
    static sf::Sprite gridSprite;
    static bool gridReady = false;
    static float prevHexSize = -1.0f;
    static sf::Vector2f prevOffset(99999.f, 99999.f);
    static sf::Vector2u prevSize(0u, 0u);

    if (prevSize != window_.getSize()) {
        gridRT.create(window_.getSize().x * 2u, window_.getSize().y * 2u);
        gridSprite.setTexture(gridRT.getTexture(), true);
        gridSprite.setScale(0.5f, 0.5f); // downscale to 1x (AA)
        prevSize = window_.getSize();
    }
    gridRT.clear(sf::Color(0, 0, 0, 0));

    // compute visible world corners
    sf::Vector2u ws = window_.getSize();
    sf::Vector2f tl(0.f, 0.f);
    sf::Vector2f tr(static_cast<float>(ws.x), 0.f);
    sf::Vector2f bl(0.f, static_cast<float>(ws.y));
    sf::Vector2f br(static_cast<float>(ws.x), static_cast<float>(ws.y));

    Axial aTL = pixelToAxial(tl - offset_, hexSize_);
    Axial aTR = pixelToAxial(tr - offset_, hexSize_);
    Axial aBL = pixelToAxial(bl - offset_, hexSize_);
    Axial aBR = pixelToAxial(br - offset_, hexSize_);

    int minQ = std::min(std::min(aTL.q, aTR.q), std::min(aBL.q, aBR.q)) - 3;
    int maxQ = std::max(std::max(aTL.q, aTR.q), std::max(aBL.q, aBR.q)) + 3;
    int minR = std::min(std::min(aTL.r, aTR.r), std::min(aBL.r, aBR.r)) - 3;
    int maxR = std::max(std::max(aTL.r, aTR.r), std::max(aBL.r, aBR.r)) + 3;

    auto gridHex = makeHex(baseSize * 2.0f);
    gridHex.setFillColor(sf::Color(0, 0, 0, 0));
    gridHex.setOutlineThickness(2.0f); // ~1px after downscale
    gridHex.setOutlineColor(sf::Color(120, 120, 130, 60));

    // draw the grid, using animated white outline for neighbor ring, grey elsewhere
    for (int q = minQ; q <= maxQ; ++q) {
        for (int r = minR; r <= maxR; ++r) {
            Axial pos{ q, r };
            Pixel p = axialToPixel(pos, hexSize_);

            // Special-case: if board is empty, highlight {0,0} as bright
            if (state_.board().empty() && q == 0 && r == 0) {
                gridHex.setFillColor(sf::Color(0, 0, 0, 0));
                gridHex.setOutlineColor(sf::Color(255, 255, 255, 120));
                gridHex.setPosition(
                    sf::Vector2f(offset_.x * 2.f, offset_.y * 2.f)
                    + sf::Vector2f(p.x * 2.f, p.y * 2.f)
                );
                gridRT.draw(gridHex);
                continue;  // skip normal grey/alpha handling for this cell
            }

            // alpha for this cell in [0..1], default 0 if not present
            float a = 0.f;
            auto itA = gridRingAlpha_.find(ringKey(pos));
            if (itA != gridRingAlpha_.end()) a = std::clamp(itA->second, 0.0f, 1.0f);

            // colors
            const sf::Color whiteOutline(255, 255, 255, static_cast<sf::Uint8>(a * 128.f)); // fade to ~128 alpha
            const sf::Color greyOutline(130, 130, 140, 50);
            const sf::Color greyFill(140, 145, 155, 22); 

            // bright ring gets transparent fill; non-bright gets grey fill
            const bool isBrightNow = (a > 0.001f);

            gridHex.setOutlineColor(isBrightNow ? whiteOutline : greyOutline);
            gridHex.setFillColor(isBrightNow ? sf::Color(0, 0, 0, 0) : greyFill);

            gridHex.setPosition(
                sf::Vector2f(offset_.x * 2.f, offset_.y * 2.f)
                + sf::Vector2f(p.x * 2.f, p.y * 2.f)
            );
            gridRT.draw(gridHex);
        }
    }
    gridRT.display();
    gridRT.setSmooth(true);
    gridReady = true;
    prevHexSize = hexSize_;
    prevOffset = offset_;
    
    rt.draw(gridSprite);
}

void UIApp::drawBoardHexes(sf::RenderTarget& rt, float baseSize) {
    auto hex = makeHex(baseSize);
    for (const auto& [pos, stack] : state_.board()) {
        Pixel px = axialToPixel(pos, hexSize_);
        hex.setPosition(offset_ + sf::Vector2f(px.x, px.y));

        if (!stack.empty()) {
            const Piece& top = state_.pieces()[stack.back()];
            hex.setFillColor(top.color == Color::White ? sf::Color(245, 245, 245) : sf::Color(30, 30, 30));
        }
        else {
            hex.setFillColor(sf::Color(236, 240, 241));
        }
        if (selectedPid_ >= 0 && !stack.empty() && stack.back() == selectedPid_) {
            hex.setOutlineColor(sf::Color::Blue);
        }
        else if (hoverAx_ && *hoverAx_ == pos) {
            hex.setOutlineColor(sf::Color(255, 180, 0));
        }
        else {
            hex.setOutlineColor(sf::Color::Black);
        }
        rt.draw(hex);
    }
}

void UIApp::drawPieceLabels(sf::RenderTarget& rt, float baseSize) {
    if (!fontOk_) return;
    for (const auto& [pos, stack] : state_.board()) {
        if (stack.empty()) continue;
        const Piece& top = state_.pieces()[stack.back()];
        char c = '?';
        switch (top.bug) {
        case Bug::Queen: c = 'Q'; break; case Bug::Ant: c = 'A'; break;
        case Bug::Spider: c = 'S'; break; case Bug::Grasshopper: c = 'G'; break;
        case Bug::Beetle: c = 'B'; break;
        }
        Pixel px = axialToPixel(pos, hexSize_);
        sf::Text t; t.setFont(font_);
        t.setCharacterSize(static_cast<unsigned>(baseSize * 0.6f));
        t.setString(std::string(1, c));
        t.setFillColor(top.color == Color::White ? sf::Color::Black : sf::Color::White);
        auto b = t.getLocalBounds();
        t.setOrigin(b.left + b.width / 2.f, b.top + b.height * 0.7f);
        t.setPosition(offset_.x + px.x, offset_.y + px.y);
        rt.draw(t);
    }
}

void UIApp::drawLegalTargets(sf::RenderTarget& rt, float baseSize) {
    for (const auto& kv : ringAlpha_) {
        float a = std::clamp(kv.second, 0.0f, 1.0f);
        if (a <= 0.001f) continue;
        Axial pos = axialFromKey(kv.first);
        Pixel p = axialToPixel(pos, hexSize_);
        auto ring = makeHex(baseSize * 0.92f);
        ring.setPosition(offset_ + sf::Vector2f(p.x, p.y));
        ring.setFillColor(sf::Color(0, 0, 0, 0));
        ring.setOutlineThickness(3.0f);
        ring.setOutlineColor(sf::Color(0, 180, 180, static_cast<sf::Uint8>(a * 255.0f)));
        rt.draw(ring);
    }
}

void UIApp::drawHoverOutline(sf::RenderTarget& rt, float baseSize) {
    if (!hoverAx_) return;
    auto it = state_.board().find(*hoverAx_);
    bool hasSelectedHere = false;
    if (it != state_.board().end() && !it->second.empty()) {
        hasSelectedHere = (selectedPid_ >= 0 && it->second.back() == selectedPid_);
    }
    if (hasSelectedHere) return;
    Pixel px = axialToPixel(*hoverAx_, hexSize_);
    auto h = makeHex(baseSize);
    h.setPosition(offset_ + sf::Vector2f(px.x, px.y));
    h.setFillColor(sf::Color(0, 0, 0, 0));
    h.setOutlineThickness(4.0f);
    h.setOutlineColor(sf::Color(255, 200, 60));
    rt.draw(h);
}

// ===== tray + placement =====
void UIApp::initReservesFromBoard() {
    auto seed = [&](std::unordered_map<hive::Bug, int>& m) {
        m[hive::Bug::Queen] = 1; m[hive::Bug::Spider] = 2; m[hive::Bug::Beetle] = 2; m[hive::Bug::Grasshopper] = 3; m[hive::Bug::Ant] = 3;
        };
    seed(remainingWhite_); seed(remainingBlack_);

    // subtract already-placed pieces from reserves
    for (size_t pid = 0; pid < state_.pieces().size(); ++pid) {
        const auto& p = state_.pieces()[pid];
        auto& rem = (p.color == hive::Color::White) ? remainingWhite_ : remainingBlack_;
        if (rem[p.bug] > 0) { rem[p.bug] -= 1; }
    }
}

// ===== placement helpers =====
bool UIApp::queenPlaced(hive::Color c) const {
    for (const auto& [pos, stack] : state_.board()) {
        if (!stack.empty()) {
            // scan full stack, not just top
            for (int pid : stack) {
                const Piece& p = state_.pieces()[pid];
                if (p.color == c && p.bug == hive::Bug::Queen) return true;
            }
        }
    }
    return false;
}

int UIApp::placementsMade(hive::Color c) const {
    // Total base Hive pieces per color = 11 (1Q, 2S, 2B, 3G, 3A)
    auto total = 11;
    const auto& rem = (c == hive::Color::White) ? remainingWhite_ : remainingBlack_;
    int remaining = 0;
    for (auto& kv : rem) remaining += kv.second;
    return total - remaining;
}

bool UIApp::adjacentToColor(hive::Axial a, hive::Color c) const {
    for (int i = 0; i < 6; ++i) {
        Axial n = add(a, dir(i));
        auto it = state_.board().find(n);
        if (it != state_.board().end() && !it->second.empty()) {
            const auto& st = it->second;
            const Piece& top = state_.pieces()[st.back()];
            if (top.color == c) return true;
        }
    }
    return false;
}

bool UIApp::adjacentToOpponent(hive::Axial a, hive::Color c) const {
    hive::Color opp = (c == hive::Color::White) ? hive::Color::Black : hive::Color::White;
    return adjacentToColor(a, opp);
}

std::vector<hive::Axial> UIApp::computePlacementTargets(hive::Color c) const {
    std::unordered_set<std::int64_t> uniq;
    std::vector<hive::Axial> out;

    if (state_.board().empty()) {
        out.push_back({ 0,0 });
        return out;
    }

    // gather all empties adjacent to the hive
    std::vector<hive::Axial> candidates;
    candidates.reserve(state_.board().size() * 3);
    for (const auto& [pos, stack] : state_.board()) {
        for (int i = 0; i < 6; ++i) {
            Axial n = add(pos, dir(i));
            if (!occupied(state_, n)) {
                auto k = ringKey(n);
                if (uniq.insert(k).second) candidates.push_back(n);
            }
        }
    }

    // if this color has no placed pieces yet, allow any empty neighbor of the hive
    if (placementsMade(c) == 0) {
        return candidates; // simple opening allowance so Black can place after White
    }

    // otherwise, enforce: must touch own color AND cannot touch opponent
    for (const auto& a : candidates) {
        if (adjacentToColor(a, c) && !adjacentToOpponent(a, c)) {
            out.push_back(a);
        }
    }
    return out;
}


bool UIApp::hitTestTray(sf::Vector2f pt, hive::Color& outColor, hive::Bug& outBug) const {
    for (const auto& it : trayItems_) {
        if (it.rect.contains(pt)) {
            outColor = it.color; outBug = it.bug; return true;
        }
    }
    return false;
}

void UIApp::drawPieceTray(sf::RenderTarget& rt) {
    trayItems_.clear();
    const float panelW = 210.f;
    const sf::Vector2u ws = window_.getSize();
    const float x0 = static_cast<float>(ws.x) - panelW;
    const float y0 = 12.f;
    const float rowH = 34.f;
    const float sectionGap = 12.f;

    // panel background
    sf::RectangleShape panel;
    panel.setPosition(sf::Vector2f(x0, 0.f));
    panel.setSize(sf::Vector2f(panelW, static_cast<float>(ws.y)));
    panel.setFillColor(sf::Color(245, 245, 248, 230));
    panel.setOutlineThickness(1.f);
    panel.setOutlineColor(sf::Color(200, 200, 210));
    rt.draw(panel);

    bool showQueenHint = (queenWarningTimer_ > 0.f);

    auto drawSection = [&](hive::Color col, float& y) {

        bool activeSection = (col == currentTurn_);
        sf::Uint8 rowAlpha = activeSection ? 255 : 160;   // dim off-turn

        if (fontOk_) {
            sf::Text t; t.setFont(font_);
            t.setCharacterSize(16);
            t.setString(col == hive::Color::White ? "White Reserve" : "Black Reserve");
            t.setFillColor(activeSection ? sf::Color(30, 30, 35, rowAlpha) : sf::Color(90, 90, 100, static_cast<sf::Uint8>(ALPHA_FADE_OFF_TURN * rowAlpha)));
            t.setPosition(x0 + 10.f, y);
            rt.draw(t);
        }
        y += 22.f;

        const std::array<hive::Bug, 5> order{ hive::Bug::Queen, hive::Bug::Spider, hive::Bug::Beetle, hive::Bug::Grasshopper, hive::Bug::Ant };
        for (auto bug : order) {
            int remaining = (col == hive::Color::White ? remainingWhite_.at(bug) : remainingBlack_.at(bug));

            sf::FloatRect box(x0 + 10.f, y, panelW - 20.f, rowH);
            // store hit rect
            trayItems_.push_back({ box, col, bug });

            sf::RectangleShape r; r.setPosition({ box.left, box.top }); r.setSize({ box.width, box.height });
            r.setFillColor(sf::Color(255, 255, 255, remaining > 0 ? rowAlpha : static_cast<sf::Uint8>(rowAlpha * ALPHA_FADE_OFF_TURN)));
            r.setOutlineThickness(1.f); r.setOutlineColor(sf::Color(190, 190, 200));
            rt.draw(r);

            if (fontOk_) {
                char c = '?';
            switch (bug) { case hive::Bug::Queen:c = 'Q'; break; case hive::Bug::Spider:c = 'S'; break; case hive::Bug::Beetle:c = 'B'; break; case hive::Bug::Grasshopper:c = 'G'; break; case hive::Bug::Ant:c = 'A'; break; }
                sf::Text t; t.setFont(font_);
                t.setCharacterSize(18);

                // Derive an alpha consistent with your row styling
                sf::Uint8 alpha = 255;

                if (!activeSection) alpha = static_cast<sf::Uint8>(alpha * 0.62f);
                if (remaining <= 0) alpha = static_cast<sf::Uint8>(alpha * 0.60f);

                // Grey-out non-Queen rows if this color has already made >4 placements and hasn't placed the Queen yet
                bool requireQueenNow = (!queenPlaced(col) && placementsMade(col) >= 3);
                if (requireQueenNow && bug != hive::Bug::Queen) {
                    // muted grey
                    t.setFillColor(sf::Color(130, 130, 140, alpha));
                }
                else {
                    // normal text color (active vs. off-turn tint)
                    t.setFillColor(activeSection ? sf::Color(30, 30, 35, alpha)
                        : sf::Color(90, 90, 100, alpha));
                }

                t.setString(std::string(1, c) + "  x" + std::to_string(remaining));
                t.setPosition(box.left + 10.f, box.top + 6.f);
                rt.draw(t);
            }

            // If we are warning about queen placement, highlight the Queen row for the active color
            if (showQueenHint && col == currentTurn_ && bug == hive::Bug::Queen) {
                sf::RectangleShape hint;
                hint.setPosition({ box.left, box.top });
                hint.setSize({ box.width, box.height });
                hint.setFillColor(sf::Color(0, 0, 0, 0));
                // pulse the alpha a bit for attention
                float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(std::fmod(queenWarningTimer_ * 10.f, 6.28318f)));
                hint.setOutlineThickness(2.f);
                hint.setOutlineColor(sf::Color(220, 40, 40, static_cast<sf::Uint8>(120 + 100 * pulse)));
                rt.draw(hint);
            }

            if (pendingPlace_ && pendingPlace_->first == col && pendingPlace_->second == bug) {
                sf::RectangleShape h; h.setPosition({ box.left, box.top }); h.setSize({ box.width, box.height });
                h.setFillColor(sf::Color(0, 180, 180, 40));
                h.setOutlineThickness(2.f); h.setOutlineColor(sf::Color(0, 180, 180, 180));
                rt.draw(h);
            }

            y += rowH + 6.f;
        }
        y += sectionGap;
        };

    float y = y0;
    drawSection(hive::Color::White, y);
    drawSection(hive::Color::Black, y);
}


// ===== render orchestrator =====
void UIApp::render() {
    window_.clear(sf::Color(250, 250, 252));

    const float kSepPct = 0.10f;
    const float shrink = hexSize_ * (kSepPct * 0.5f);
    const float baseSize = std::max(6.0f, hexSize_ - shrink);

    drawBackgroundGrid(window_, baseSize);
    drawBoardHexes(window_, baseSize);
    drawPieceLabels(window_, baseSize);
    drawLegalTargets(window_, baseSize);
    drawHoverOutline(window_, baseSize);
    drawPieceTray(window_);

    if (fontOk_) {
        sf::Text turn; turn.setFont(font_);
        turn.setCharacterSize(16);
        turn.setString(currentTurn_ == hive::Color::White ? "White to move" : "Black to move");
        turn.setFillColor(sf::Color(220, 220, 220));
        turn.setPosition(10.f, 10.f);

        // measure text
        sf::FloatRect tb = turn.getLocalBounds();
        float padding = 6.f;

        sf::RectangleShape bg;
        bg.setPosition(turn.getPosition().x - padding, turn.getPosition().y - padding);
        bg.setSize(sf::Vector2f(tb.width + 2 * padding, tb.height + 3 * padding));
        bg.setFillColor(sf::Color(55, 55, 55, 220)); // light background
        bg.setOutlineThickness(1.f);
        bg.setOutlineColor(sf::Color(200, 200, 210));

        // draw rectangle first, then text
        window_.draw(bg);
        window_.draw(turn);
    }

    // helper lambda inside UIApp::render()
    auto drawOverlay = [&](const std::string& text, sf::Color color, float timer, float duration, float yOffset = 0.f) {
        if (timer > 0.f && fontOk_) {
            float alpha = std::clamp((timer / duration) * 255.f, 0.f, 255.f);

            sf::Text msg;
            msg.setFont(font_);
            msg.setCharacterSize(28);
            msg.setString(text);
            msg.setFillColor(sf::Color(color.r, color.g, color.b, static_cast<sf::Uint8>(alpha)));

            sf::FloatRect tb = msg.getLocalBounds();
            float cx = (window_.getSize().x - tb.width) / 2.f;
            float cy = (window_.getSize().y - tb.height) / 2.f + yOffset;
            msg.setPosition(cx, cy);

            sf::RectangleShape bg;
            float pad = 10.f;
            bg.setPosition(cx - pad, cy - pad);
            bg.setSize(sf::Vector2f(tb.width + 2 * pad, tb.height + 2 * pad));
            bg.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(alpha * 0.25f)));
            bg.setOutlineThickness(1.f);
            bg.setOutlineColor(sf::Color(200, 200, 210, static_cast<sf::Uint8>(alpha)));

            window_.draw(bg);
            window_.draw(msg);
        }
        };

    // --- overlays ---
    drawOverlay("Must place Queen by 4th turn!", sf::Color(255, 0, 0), queenWarningTimer_, OVERLAY_Q_BY4_SEC);
    drawOverlay("Place your Queen before moving.", sf::Color(255, 80, 0), moveBeforeQueenTimer_, OVERLAY_MOVE_BEFORE_Q_SEC, 44.f);

    window_.display();
}
