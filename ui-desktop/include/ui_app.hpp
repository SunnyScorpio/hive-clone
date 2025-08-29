#pragma once
#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <optional>
#include <vector>
#include <cstdint>
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

	// UI tray
	void drawPieceTray(sf::RenderTarget& rt);

	// turn management
	hive::Color currentTurn_{ hive::Color::White };
	// helper to toggle turns
	inline void nextTurn() {
		currentTurn_ = (currentTurn_ == hive::Color::White) ? hive::Color::Black
			: hive::Color::White;
	}

	// Placement / reserves
	void initReservesFromBoard();
	std::vector<hive::Axial> computePlacementTargets(hive::Color c) const;
	bool hitTestTray(sf::Vector2f pt, hive::Color& outColor, hive::Bug& outBug) const;

	struct TrayItem { sf::FloatRect rect; hive::Color color; hive::Bug bug; };
	mutable std::vector<TrayItem> trayItems_;

	// piece reserves and placement
	std::unordered_map<hive::Bug, int> remainingWhite_;
	std::unordered_map<hive::Bug, int> remainingBlack_;
	std::optional<std::pair<hive::Color, hive::Bug>> pendingPlace_;

	// placement helpers
	bool queenPlaced(hive::Color c) const;
	int  placementsMade(hive::Color c) const;
	bool adjacentToColor(hive::Axial a, hive::Color c) const;
	bool adjacentToOpponent(hive::Axial a, hive::Color c) const;

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
	std::unordered_map<std::int64_t, float> ringAlpha_; // key: packed (q,r) -> alpha [0..1]

	// feedback
	float queenWarningTimer_{ 0.f }; // counts down from 2.0f when warning about missing queen
	float moveBeforeQueenTimer_{ 0.f };   //fade-out warning when moving before queen

	// animated alpha for the "white neighbor grid" ring (like teal rings)
	std::unordered_map<std::int64_t, float> gridRingAlpha_;  // key: packed (q,r) -> alpha [0..1]
};