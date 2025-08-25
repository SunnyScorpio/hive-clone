#include "ui_app.hpp"
#include <cmath>
#include <numbers>
#include <algorithm>
#include <unordered_set>
#include <cstdint>
#include <array>
#include <string>

using namespace hive;

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

    // demo pieces (no Beetle for now)
    state_.addDemoPiece(Bug::Queen, Color::White, { 0,0 });
    state_.addDemoPiece(Bug::Ant, Color::White, { 1,0 });
    state_.addDemoPiece(Bug::Spider, Color::Black, { 0,1 });
    state_.addDemoPiece(Bug::Grasshopper, Color::White, { -1,1 });

    fontOk_ = font_.loadFromFile("assets/DejaVuSans.ttf");
    offset_ = sf::Vector2f(512.f, 384.f);

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
            if (hitTestTray(screenPt, hitColor, hitBug)) {
                // arm pending placement if we have remaining pieces of that kind
                auto& rem = (hitColor == hive::Color::White) ? remainingWhite_ : remainingBlack_;
                if (rem[hitBug] > 0) {
                    pendingPlace_ = std::make_pair(hitColor, hitBug);
                    // compute fresh legal placement targets for this color
                    legalTargets_ = computePlacementTargets(hitColor);
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
                        state_.movePiece(selectedPid_, clickAx, /*allowStack=*/true);
                        selectedPid_ = -1;
                        legalTargets_.clear();
                    }
                }
            }
            else {
                // No selection yet: select top piece at click location, if any
                auto it = state_.board().find(clickAx);
                if (it != state_.board().end() && !it->second.empty()) {
                    selectedPid_ = it->second.back();
                    legalTargets_.clear();
                    for (const auto& mv : legalMovesForPiece(state_, selectedPid_)) {
                        legalTargets_.push_back(mv.to);
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

    const float kRate = 0.20f; // smoothing factor per frame

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
};

void UIApp::drawBackgroundGrid(sf::RenderTarget& rt, float baseSize) {
    // cached, faint grid with AA via 2x supersample
    static sf::RenderTexture gridRT;
    static sf::Sprite gridSprite;
    static bool gridReady = false;
    static float prevHexSize = -1.0f;
    static sf::Vector2f prevOffset(99999.f, 99999.f);
    static sf::Vector2u prevSize(0u, 0u);

    const bool sizeChanged = (prevSize != window_.getSize());
    const bool zoomChanged = (prevHexSize != hexSize_);
    const bool panChanged = (prevOffset.x != offset_.x || prevOffset.y != offset_.y);

    if (!gridReady || sizeChanged || zoomChanged || panChanged) {
        if (sizeChanged) {
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

        for (int q = minQ; q <= maxQ; ++q) {
            for (int r = minR; r <= maxR; ++r) {
                Pixel p = axialToPixel({ q,r }, hexSize_);
                gridHex.setPosition(sf::Vector2f(offset_.x * 2.f, offset_.y * 2.f) + sf::Vector2f(p.x * 2.f, p.y * 2.f));
                gridRT.draw(gridHex);
            }
        }
        gridRT.display();
        gridRT.setSmooth(true);
        gridReady = true;
        prevHexSize = hexSize_;
        prevOffset = offset_;
    }
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

std::vector<hive::Axial> UIApp::computePlacementTargets(hive::Color /*c*/) const {
    // Simplified base rule: any empty hex adjacent to the hive (no expansions / full constraints yet)
    std::unordered_set<std::int64_t> uniq;
    std::vector<hive::Axial> out;

    if (state_.board().empty()) {
        out.push_back({ 0,0 });
        return out;
    }
    for (const auto& [pos, stack] : state_.board()) {
        for (int i = 0; i < 6; ++i) {
            Axial n = add(pos, dir(i));
            if (!occupied(state_, n)) {
                auto k = ringKey(n);
                if (uniq.insert(k).second) out.push_back(n);
            }
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

    auto drawSection = [&](hive::Color col, float& y) {
        if (fontOk_) {
            sf::Text t; t.setFont(font_);
            t.setCharacterSize(16);
            t.setString(col == hive::Color::White ? "White Reserve" : "Black Reserve");
            t.setFillColor(sf::Color(60, 60, 70));
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
            r.setFillColor(sf::Color(255, 255, 255, remaining > 0 ? 255 : 120));
            r.setOutlineThickness(1.f); r.setOutlineColor(sf::Color(190, 190, 200));
            rt.draw(r);

            if (fontOk_) {
                char c = '?';
        switch (bug) { case hive::Bug::Queen:c = 'Q'; break; case hive::Bug::Spider:c = 'S'; break; case hive::Bug::Beetle:c = 'B'; break; case hive::Bug::Grasshopper:c = 'G'; break; case hive::Bug::Ant:c = 'A'; break; }
                                            sf::Text t; t.setFont(font_);
                                            t.setCharacterSize(18);
                                            t.setFillColor(sf::Color(30, 30, 35));
                                            t.setString(std::string(1, c) + "  x" + std::to_string(remaining));
                                            t.setPosition(box.left + 10.f, box.top + 6.f);
                                            rt.draw(t);
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

    window_.display();
}
