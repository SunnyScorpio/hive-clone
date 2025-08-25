#pragma once
#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <optional>
#include <vector>
#include "engine.hpp"
#include "rules.hpp"


class UIApp {
public:
	UIApp();
	void run();


private:
	// lifecycle
	void handleEvents();
	void update();
	void render();


	// helpers
	static sf::ConvexShape makeHex(float size);
	static hive::Axial pixelToAxial(sf::Vector2f p, float s);


	// render helpers (kept private)
	void drawBackgroundGrid(sf::RenderTarget& rt, float baseSize);
	void drawBoardHexes(sf::RenderTarget& rt, float baseSize);
	void drawPieceLabels(sf::RenderTarget& rt, float baseSize);
	void drawLegalTargets(sf::RenderTarget& rt, float baseSize);
	void drawHoverOutline(sf::RenderTarget& rt, float baseSize);


	// ring animation helpers
	static std::int64_t ringKey(hive::Axial a);
	static hive::Axial axialFromKey(std::int64_t k);


	// data


	// data
	sf::RenderWindow window_;
	hive::GameState state_;


	float hexSize_{ 40.0f };
	sf::Font font_; bool fontOk_{ false };
	sf::Vector2f offset_;
	bool dragging_{ false };
	sf::Vector2i lastMouse_{};


	int selectedPid_{ -1 };
	std::optional<hive::Axial> hoverAx_;
	std::unordered_map<int, sf::Vector2f> animPos_;


	// rules/UI
	std::vector<hive::Axial> legalTargets_;
	std::unordered_map<std::int64_t, float> ringAlpha_; // key: packed (q,r) -> alpha [0..1] // key: packed (q,r) -> alpha [0..1]
};