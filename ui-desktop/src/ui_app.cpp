#include "ui_app.hpp"
#include <cmath>
#include <numbers>

using namespace hive;

// === helpers ===
sf::ConvexShape UIApp::makeHex(float size) {
    sf::ConvexShape hex; hex.setPointCount(6);
    for (int i = 0; i < 6; ++i) {
        float angle = static_cast<float>(std::numbers::pi) / 180.0f * (60.0f * i - 30.0f);
        hex.setPoint(i, sf::Vector2f(std::cos(angle) * size, std::sin(angle) * size));
    }
    hex.setOutlineThickness(3.0f); // thicker outlines
    hex.setOutlineColor(sf::Color::Black);
    hex.setFillColor(sf::Color(235, 235, 235));
    return hex;
}

static hive::Axial cubeToAxial(float x, float z) { return { (int)std::round(x), (int)std::round(z) }; }

hive::Axial UIApp::pixelToAxial(sf::Vector2f p, float s) {
    constexpr float SQ3 = 1.7320508075688772f;
    float q = (p.x * (1.0f / SQ3) / s) - (p.y * (1.0f / 3.0f) / s);
    float r = (2.0f / 3.0f) * (p.y / s);
    // cube round
    float x = q, z = r, y = -x - z;
    float rx = std::round(x), ry = std::round(y), rz = std::round(z);
    float x_diff = std::fabs(rx - x);
    float y_diff = std::fabs(ry - y);
    float z_diff = std::fabs(rz - z);
    if (x_diff > y_diff && x_diff > z_diff) rx = -ry - rz;
    else if (y_diff > z_diff) ry = -rx - rz;
    else rz = -rx - ry;
    return cubeToAxial(rx, rz);
}

UIApp::UIApp() : window_(sf::VideoMode(1024, 768), "Hive (Desktop) � Kickstart", sf::Style::Default, sf::ContextSettings(0, 0, 8)) {
    // Enable anti-aliasing with 8x samples
    window_.setFramerateLimit(60);

    // Demo hive (no Beetle for now)
    state_.addDemoPiece(Bug::Queen, Color::White, { 0,0 });
    state_.addDemoPiece(Bug::Ant, Color::White, { 1,0 });
    state_.addDemoPiece(Bug::Spider, Color::Black, { 0,1 });
    state_.addDemoPiece(Bug::Grasshopper, Color::White, { -1,1 });

    fontOk_ = font_.loadFromFile("assets/DejaVuSans.ttf");
    offset_ = { 512, 384 };
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
        if (e.type == sf::Event::MouseWheelScrolled) {
            float dz = e.mouseWheelScroll.delta;
            hexSize_ = std::max(10.0f, std::min(120.0f, hexSize_ + dz * 5.0f));
            animPos_.clear();
        }
        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Right) {
            dragging_ = true; lastMouse_ = sf::Mouse::getPosition(window_);
        }
        if (e.type == sf::Event::MouseButtonReleased && e.mouseButton.button == sf::Mouse::Right) {
            dragging_ = false;
        }
        if (e.type == sf::Event::MouseMoved && dragging_) {
            auto cur = sf::Mouse::getPosition(window_);
            offset_ += sf::Vector2f(float(cur.x - lastMouse_.x), float(cur.y - lastMouse_.y));
            lastMouse_ = cur;
            animPos_.clear();
        }
        if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Left) {
            if (hoverAx_) {
                if (selectedPid_ >= 0) {
                    state_.movePiece(selectedPid_, *hoverAx_, /*allowStack=*/true);
                    selectedPid_ = -1;
                    animPos_.clear();
                }
                else {
                    auto it = state_.board().find(*hoverAx_);
                    if (it != state_.board().end() && !it->second.empty()) {
                        selectedPid_ = it->second.back();
                    }
                }
            }
        }
    }
}

void UIApp::update() {
    // Update hover axial from mouse
    sf::Vector2i m = sf::Mouse::getPosition(window_);
    sf::Vector2f world = sf::Vector2f(m.x, m.y) - offset_;
    hoverAx_ = pixelToAxial(world, hexSize_);
}

void UIApp::render() {
    window_.clear(sf::Color(250, 250, 252));

    auto hex = makeHex(hexSize_);
    // Draw board hexes
    for (const auto& [pos, stack] : state_.board()) {
        Pixel px = axialToPixel(pos, hexSize_);
        hex.setPosition(offset_ + sf::Vector2f(px.x, px.y));

        // Color the entire hex based on top piece color
        if (!stack.empty()) {
            const Piece& top = state_.pieces()[stack.back()];
            hex.setFillColor(top.color == Color::White ? sf::Color(245, 245, 245) : sf::Color(30, 30, 30));
        }
        else {
            hex.setFillColor(sf::Color(236, 240, 241));
        }

        // Outline priority: selected tile (blue) > hover (gold) > default (black)
        if (selectedPid_ >= 0 && !stack.empty() && stack.back() == selectedPid_) {
            hex.setOutlineColor(sf::Color::Blue);
        }
        else if (hoverAx_ && *hoverAx_ == pos) {
            hex.setOutlineColor(sf::Color(255, 180, 0));
        }
        else {
            hex.setOutlineColor(sf::Color::Black);
        }
        window_.draw(hex);

        // Centered letter label on occupied hexes
        if (fontOk_ && !stack.empty()) {
            const Piece& top = state_.pieces()[stack.back()];
            char c = '?';
            switch (top.bug) {
            case Bug::Queen: c = 'Q'; break; case Bug::Ant: c = 'A'; break;
            case Bug::Spider: c = 'S'; break; case Bug::Grasshopper: c = 'G'; break;
            case Bug::Beetle: c = 'B'; break;
            }
            sf::Text t; t.setFont(font_);
            t.setCharacterSize(static_cast<unsigned>(hexSize_ * 0.6f)); // scale with zoom
            t.setString(std::string(1, c));
            t.setFillColor(top.color == Color::White ? sf::Color::Black : sf::Color::White);
            auto b = t.getLocalBounds();
            t.setOrigin(b.left + b.width / 2.f, b.top + b.height * 0.7f);
            t.setPosition(offset_.x + px.x, offset_.y + px.y);
            window_.draw(t);
        }
    }

    // Hover outline on empty cells too (when hovering an empty pos)
    if (hoverAx_) {
        // Only draw the extra hover ring if nothing was drawn for selection above
        auto it = state_.board().find(*hoverAx_);
        bool hasSelectedHere = false;
        if (it != state_.board().end() && !it->second.empty()) {
            hasSelectedHere = (selectedPid_ >= 0 && it->second.back() == selectedPid_);
        }
        if (!hasSelectedHere) {
            Pixel px = axialToPixel(*hoverAx_, hexSize_);
            auto h = makeHex(hexSize_);
            h.setPosition(offset_ + sf::Vector2f(px.x, px.y));
            h.setFillColor(sf::Color(0, 0, 0, 0));
            h.setOutlineThickness(4.0f); // thicker hover outline
            h.setOutlineColor(sf::Color(255, 200, 60));
            window_.draw(h);
        }
    }

    window_.display();
}