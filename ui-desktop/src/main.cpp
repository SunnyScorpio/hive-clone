#include <SFML/Graphics.hpp>
#include <cmath>
#include <numbers>
#include <vector>
#include <string>
#include "engine.hpp"

using namespace hive;

static sf::ConvexShape makeHex(float size) {
    sf::ConvexShape hex; hex.setPointCount(6);
    // pointy-top hexagon
    for (int i = 0; i < 6; ++i) {
        float angle = static_cast<float>(std::numbers::pi / 180.0) * (60.0f * i - 30.0f);
        hex.setPoint(i, sf::Vector2f(std::cos(angle) * size, std::sin(angle) * size));
    }
    hex.setOutlineThickness(1.5f);
    hex.setOutlineColor(sf::Color::Black);
    hex.setFillColor(sf::Color(235, 235, 235));
    return hex;
}

int main() {
    const unsigned W = 1024, H = 768;
    sf::RenderWindow window(sf::VideoMode(W, H), "Hive (Desktop) – Kickstart");
    window.setFramerateLimit(60);

    GameState state;
    // Demo: a tiny hive to visualize stacks
    state.addDemoPiece(Bug::Queen, Color::White, { 0,0 });
    state.addDemoPiece(Bug::Ant, Color::White, { 1,0 });
    state.addDemoPiece(Bug::Beetle, Color::Black, { 0,0 }, /*height=*/1); // stacked on queen
    state.addDemoPiece(Bug::Spider, Color::Black, { 0,1 });
    state.addDemoPiece(Bug::Grasshopper, Color::White, { -1,1 });

    float hexSize = 40.0f;
    sf::Font font;
    // Use a built-in-ish fallback: try to load system default; ignore if missing.
    font.loadFromFile("C:/Windows/Fonts/Arial/Arial Regular.ttf");

    sf::View view = window.getDefaultView();
    sf::Vector2f offset(W * 0.5f, H * 0.5f);

    bool dragging = false; sf::Vector2i lastMouse;

    while (window.isOpen()) {
        sf::Event e;
        while (window.pollEvent(e)) {
            if (e.type == sf::Event::Closed) window.close();
            if (e.type == sf::Event::MouseWheelScrolled) {
                float dz = e.mouseWheelScroll.delta;
                hexSize = std::max(10.0f, std::min(120.0f, hexSize + dz * 5.0f));
            }
            if (e.type == sf::Event::MouseButtonPressed && e.mouseButton.button == sf::Mouse::Right) {
                dragging = true; lastMouse = sf::Mouse::getPosition(window);
            }
            if (e.type == sf::Event::MouseButtonReleased && e.mouseButton.button == sf::Mouse::Right) {
                dragging = false;
            }
            if (e.type == sf::Event::MouseMoved && dragging) {
                auto cur = sf::Mouse::getPosition(window);
                offset += sf::Vector2f(float(cur.x - lastMouse.x), float(cur.y - lastMouse.y));
                lastMouse = cur;
            }
        }

        window.clear(sf::Color(250, 250, 252));

        // Draw current stacks
        auto hex = makeHex(hexSize);
        for (const auto& [pos, stack] : state.board()) {
            Pixel px = axialToPixel(pos, hexSize);
            hex.setPosition(offset + sf::Vector2f(px.x, px.y));
            hex.setFillColor(sf::Color(236, 240, 241));
            window.draw(hex);

            // draw stack as small circles with letters
            float lift = -hexSize * 0.6f; // draw stacks slightly above center
            for (int i = 0; i < (int)stack.size(); ++i) {
                const Piece& p = state.pieces()[stack[i]];
                sf::CircleShape chip(hexSize * 0.32f);
                chip.setOrigin(chip.getRadius(), chip.getRadius());
                chip.setPosition(offset + sf::Vector2f(px.x, px.y + lift - i * 18.0f));
                chip.setOutlineThickness(2.0f);
                chip.setOutlineColor(sf::Color::Black);
                chip.setFillColor(p.color == Color::White ? sf::Color(245, 245, 245) : sf::Color(30, 30, 30));
                window.draw(chip);

                // Label: Q, A, B, S, G
                char c = '?';
                switch (p.bug) {
                case Bug::Queen: c = 'Q'; break; case Bug::Ant: c = 'A'; break; case Bug::Beetle: c = 'B'; break;
                case Bug::Spider: c = 'S'; break; case Bug::Grasshopper: c = 'G'; break;
                }
                if (font.getInfo().family.size()) {
                    sf::Text t; t.setFont(font); t.setCharacterSize(18);
                    t.setString(std::string(1, c));
                    t.setFillColor(p.color == Color::White ? sf::Color::Black : sf::Color::White);
                    auto b = t.getLocalBounds();
                    t.setOrigin(b.left + b.width / 2.0f, b.top + b.height * 0.7f);
                    t.setPosition(offset + sf::Vector2f(px.x, px.y + lift - i * 18.0f));
                    window.draw(t);
                }
            }
        }

        window.display();
    }
    return 0;
}