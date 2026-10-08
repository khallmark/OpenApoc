#include "game/ui/tileview/tileview.h"
#include "framework/configfile.h"
#include "framework/event.h"
#include "framework/framework.h"
#include "framework/options.h"
#include "framework/keycodes.h"
#include "framework/renderer.h"
#include "framework/sound.h"
#include "game/state/battle/battle.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace OpenApoc
{

TileView::TileView(TileMap &map, Vec3<int> isoTileSize, Vec2<int> stratTileSize,
                   TileViewMode initialMode)
    : Stage(), map(map), isoTileSize(isoTileSize), stratTileSize(stratTileSize),
      viewMode(initialMode), dpySize(fw().displayGetWidth(), fw().displayGetHeight()),
      strategyViewBoxColour(212, 176, 172, 255), strategyViewBoxThickness(2.0f),
      selectedTilePosition(0, 0, 0), maxZDraw(map.size.z), centerPos(0, 0, 0)
{
	LogInfo("dpySize: {0}", dpySize);
}

TileView::~TileView() = default;

// Edge-of-screen "autoscroll" (see EVENT_MOUSE_MOVE below) latches a scroll direction until an
// *opposing* mouse move arrives - a real pointer always eventually moves away from the edge, but
// a touch just lifts. A tap or drag that lands or ends inside MOUSE_SCROLL_MARGIN would latch a
// scroll nothing then clears, so it's meaningless - and actively broken - without a hovering
// pointer. Touch pans by dragging instead (EVENT_FINGER_MOVE below).
static bool defaultAutoScroll()
{
#if defined(__APPLE__) && TARGET_OS_IPHONE
	return false;
#else
	return config().getBool("Options.Misc.AutoScroll");
#endif
}

void TileView::begin() { autoScroll = defaultAutoScroll(); }

void TileView::pause()
{
	scrollUpKB = false;
	scrollDownKB = false;
	scrollLeftKB = false;
	scrollRightKB = false;
}

void TileView::resume() { autoScroll = defaultAutoScroll(); }

void TileView::finish() {}

void TileView::eventOccurred(Event *e)
{
	if (e->type() == EVENT_KEY_DOWN)
	{
		switch (e->keyboard().KeyCode)
		{
			case SDLK_F1:
				debugHotkeyMode = !debugHotkeyMode;
				LogWarning("DEBUG MODE {0}", debugHotkeyMode);
				break;
			case SDLK_UP:
				scrollUpKB = true;
				break;
			case SDLK_DOWN:
				scrollDownKB = true;
				break;
			case SDLK_LEFT:
				scrollLeftKB = true;
				break;
			case SDLK_RIGHT:
				scrollRightKB = true;
				break;
			case SDLK_s:
				if (debugHotkeyMode)
				{
					if (selectedTilePosition.y < (map.size.y - 1))
						this->setSelectedTilePosition({selectedTilePosition.x,
						                               selectedTilePosition.y + 1,
						                               selectedTilePosition.z});
				}
				else
				{
					scrollDownKB = true;
				}
				break;
			case SDLK_w:
				if (debugHotkeyMode)
				{
					if (selectedTilePosition.y > 0)
						this->setSelectedTilePosition({selectedTilePosition.x,
						                               selectedTilePosition.y - 1,
						                               selectedTilePosition.z});
				}
				else
				{
					scrollUpKB = true;
				}
				break;
			case SDLK_a:
				if (debugHotkeyMode)
				{
					if (selectedTilePosition.x > 0)
						this->setSelectedTilePosition({selectedTilePosition.x - 1,
						                               selectedTilePosition.y,
						                               selectedTilePosition.z});
				}
				else
				{
					scrollLeftKB = true;
				}
				break;
			case SDLK_d:
				if (debugHotkeyMode)
				{
					if (selectedTilePosition.x < (map.size.x - 1))
						this->setSelectedTilePosition({selectedTilePosition.x + 1,
						                               selectedTilePosition.y,
						                               selectedTilePosition.z});
				}
				else
				{
					scrollRightKB = true;
				}
				break;
			case SDLK_r:
				if (debugHotkeyMode)
				{
					if (selectedTilePosition.z < (map.size.z - 1))
						this->setSelectedTilePosition({selectedTilePosition.x,
						                               selectedTilePosition.y,
						                               selectedTilePosition.z + 1});
				}
				break;
			case SDLK_f:
				if (debugHotkeyMode)
				{
					if (selectedTilePosition.z > 0)
						this->setSelectedTilePosition({selectedTilePosition.x,
						                               selectedTilePosition.y,
						                               selectedTilePosition.z - 1});
				}
				break;
		}
	}
	else if (e->type() == EVENT_KEY_UP)
	{
		switch (e->keyboard().KeyCode)
		{
			case SDLK_UP:
				scrollUpKB = false;
				break;
			case SDLK_DOWN:
				scrollDownKB = false;
				break;
			case SDLK_LEFT:
				scrollLeftKB = false;
				break;
			case SDLK_RIGHT:
				scrollRightKB = false;
				break;
			case SDLK_w:
				scrollUpKB = false;
				break;
			case SDLK_s:
				scrollDownKB = false;
				break;
			case SDLK_a:
				scrollLeftKB = false;
				break;
			case SDLK_d:
				scrollRightKB = false;
				break;
		}
	}
	else if (e->type() == EVENT_WINDOW_DEACTIVATE)
	{
		scrollUpKB = false;
		scrollDownKB = false;
		scrollLeftKB = false;
		scrollRightKB = false;
	}
	else if (e->type() == EVENT_FINGER_MOVE)
	{
		// FIXME: Review this code for sanity
		if (e->finger().IsPrimary)
		{
			Vec3<float> deltaPos(e->finger().DeltaX, e->finger().DeltaY, 0);
			if (this->viewMode == TileViewMode::Isometric)
			{
				deltaPos.x /= isoTileSize.x;
				deltaPos.y /= isoTileSize.y;
				Vec3<float> isoDelta(deltaPos.x + deltaPos.y, deltaPos.y - deltaPos.x, 0);
				deltaPos = isoDelta;
			}
			else
			{
				deltaPos.x /= stratTileSize.x;
				deltaPos.y /= stratTileSize.y;
			}
			Vec3<float> newPos = this->centerPos - deltaPos;
			this->setScreenCenterTile(newPos);
		}
	}
	else if (e->type() == EVENT_WINDOW_RESIZE)
	{
		refreshDisplaySize();
	}
	else if (e->type() == EVENT_MOUSE_MOVE)
	{
		scrollLeftM = autoScroll && e->mouse().X < MOUSE_SCROLL_MARGIN;
		scrollRightM = autoScroll && e->mouse().X >= dpySize.x - MOUSE_SCROLL_MARGIN;
		scrollUpM = autoScroll && e->mouse().Y < MOUSE_SCROLL_MARGIN;
		scrollDownM = autoScroll && e->mouse().Y >= dpySize.y - MOUSE_SCROLL_MARGIN;
	}
	else if (e->type() == EVENT_WINDOW_DEACTIVATE)
	{
		// These latch until an opposing event arrives. Switching away with the pointer at
		// a screen edge, or with a scroll key held, would otherwise leave the map drifting
		// in the background - the matching key release goes to whichever app took focus.
		scrollLeftM = scrollRightM = scrollUpM = scrollDownM = false;
		scrollLeftKB = scrollRightKB = scrollUpKB = scrollDownKB = false;
	}
}

bool TileView::isTransition() { return false; }

void TileView::setViewMode(TileViewMode newMode) { this->viewMode = newMode; }

TileViewMode TileView::getViewMode() const { return this->viewMode; }

Vec2<int> TileView::getScreenOffset() const
{
	Vec2<float> screenOffset = this->tileToScreenCoords(this->centerPos);

	return Vec2<int>{dpySize.x / 2 - screenOffset.x, dpySize.y / 2 - screenOffset.y};
}

void TileView::setScreenCenterTile(Vec3<float> center)
{
	fw().soundBackend->setListenerPosition({center.x, center.y, map.size.z / 2});
	Vec3<float> clampedCenter;
	if (center.x < 0.0f)
		clampedCenter.x = 0.0f;
	else if (center.x > map.size.x)
		clampedCenter.x = map.size.x;
	else
		clampedCenter.x = center.x;
	if (center.y < 0.0f)
		clampedCenter.y = 0.0f;
	else if (center.y > map.size.y)
		clampedCenter.y = map.size.y;
	else
		clampedCenter.y = center.y;
	if (center.z < 0.0f)
		clampedCenter.z = 0.0f;
	else if (center.z > map.size.z)
		clampedCenter.z = map.size.z;
	else
		clampedCenter.z = center.z;

	this->centerPos = clampedCenter;
}

void TileView::setScreenCenterTile(Vec2<float> center)
{
	this->setScreenCenterTile(Vec3<float>{center.x, center.y, 1});
}

Vec3<int> TileView::getSelectedTilePosition() { return selectedTilePosition; }

void TileView::setSelectedTilePosition(Vec3<int> newPosition)
{
	selectedTilePosition = newPosition;
	if (selectedTilePosition.x < 0)
		selectedTilePosition.x = 0;
	if (selectedTilePosition.y < 0)
		selectedTilePosition.y = 0;
	if (selectedTilePosition.z < 0)
		selectedTilePosition.z = 0;
	if (selectedTilePosition.x >= map.size.x)
		selectedTilePosition.x = map.size.x - 1;
	if (selectedTilePosition.y >= map.size.y)
		selectedTilePosition.y = map.size.y - 1;
	if (selectedTilePosition.z >= map.size.z)
		selectedTilePosition.z = map.size.z - 1;
}

void TileView::applyScrolling()
{
	// Where the input wants to go: a speed in screen pixels per second, the same in every
	// direction. It used to be a step in tiles per update, so the isometric view moved twice as
	// fast sideways as up and down, and everything moved faster the faster the simulation ran.
	constexpr float EASE_SECONDS = 0.06f; // time constant to reach full speed, and to stop
	constexpr float MAX_STEP_SECONDS = 0.1f;
	const auto now = std::chrono::steady_clock::now();
	float dt = 0.0f;
	if (lastScrollTime != std::chrono::steady_clock::time_point{})
	{
		dt = std::min(MAX_STEP_SECONDS,
		              std::chrono::duration<float>(now - lastScrollTime).count());
	}
	lastScrollTime = now;
	// Frames reach the screen on the display's beat, not when they started drawing, so a step
	// measured between draw starts jitters by up to a period and so does the motion. Count whole
	// render periods instead: every displayed frame then moves the view by the same amount.
	const float period = static_cast<float>(fw().renderPeriodSeconds());
	if (period > 0.0f && dt > 0.0f)
	{
		dt = std::max(1.0f, std::round(dt / period)) * period;
	}

	Vec2<float> wanted{0.0f, 0.0f};
	wanted.x -= (scrollLeftKB || scrollLeftM) ? 1.0f : 0.0f;
	wanted.x += (scrollRightKB || scrollRightM) ? 1.0f : 0.0f;
	wanted.y -= (scrollUpKB || scrollUpM) ? 1.0f : 0.0f;
	wanted.y += (scrollDownKB || scrollDownM) ? 1.0f : 0.0f;
	if (wanted.x != 0.0f || wanted.y != 0.0f)
	{
		wanted =
		    glm::normalize(wanted) * static_cast<float>(std::max(1, Options::scrollSpeed.get()));
	}
	// Ease towards it, so starting and stopping glide instead of snapping.
	scrollVelocity += (wanted - scrollVelocity) * (1.0f - std::exp(-dt / EASE_SECONDS));
	if (wanted.x == 0.0f && wanted.y == 0.0f && glm::length(scrollVelocity) < 1.0f)
	{
		scrollVelocity = {0.0f, 0.0f};
	}
	if (scrollVelocity.x == 0.0f && scrollVelocity.y == 0.0f)
	{
		return;
	}

	// Screen movement this frame, mapped back to tiles through the view's own projection.
	const Vec2<float> d = scrollVelocity * dt;
	Vec3<float> newPos = this->centerPos;
	if (this->viewMode == TileViewMode::Isometric)
	{
		// tileToScreenCoords: x = (tx - ty) * w/2, y = (tx + ty) * h/2 at a fixed level.
		const float a = d.x / (isoTileSize.x / 2.0f);
		const float b = d.y / (isoTileSize.y / 2.0f);
		newPos.x += (a + b) / 2.0f;
		newPos.y += (b - a) / 2.0f;
	}
	else if (this->viewMode == TileViewMode::Strategy)
	{
		newPos.x += d.x / stratTileSize.x;
		newPos.y += d.y / stratTileSize.y;
	}
	else
	{
		LogError("Unknown view mode");
	}

	this->setScreenCenterTile(newPos);
}

int TileView::uiAnimationSteps()
{
	const auto now = std::chrono::steady_clock::now();
	if (lastUiAnimation == std::chrono::steady_clock::time_point{})
	{
		lastUiAnimation = now;
		return 1;
	}
	uiAnimationCarry +=
	    std::min(0.25, std::chrono::duration<double>(now - lastUiAnimation).count()) * 60.0;
	lastUiAnimation = now;
	const int steps = static_cast<int>(uiAnimationCarry);
	uiAnimationCarry -= steps;
	return steps;
}

void TileView::renderStrategyOverlay(Renderer &r)
{
	if (this->viewMode == TileViewMode::Strategy)
	{
		Vec2<float> centerIsoScreenPos = this->tileToScreenCoords(
		    Vec3<float>{this->centerPos.x, this->centerPos.y, 0}, TileViewMode::Isometric);

		/* Draw the rectangle of where the isometric view would be */
		Vec2<float> topLeftIsoScreenPos = centerIsoScreenPos;
		topLeftIsoScreenPos.x -= dpySize.x / 2;
		topLeftIsoScreenPos.y -= dpySize.y / 2;

		Vec2<float> topRightIsoScreenPos = centerIsoScreenPos;
		topRightIsoScreenPos.x += dpySize.x / 2;
		topRightIsoScreenPos.y -= dpySize.y / 2;

		Vec2<float> bottomLeftIsoScreenPos = centerIsoScreenPos;
		bottomLeftIsoScreenPos.x -= dpySize.x / 2;
		bottomLeftIsoScreenPos.y += dpySize.y / 2;

		Vec2<float> bottomRightIsoScreenPos = centerIsoScreenPos;
		bottomRightIsoScreenPos.x += dpySize.x / 2;
		bottomRightIsoScreenPos.y += dpySize.y / 2;

		Vec3<float> topLeftIsoTilePos =
		    this->screenToTileCoords(topLeftIsoScreenPos, 0.0f, TileViewMode::Isometric);
		Vec3<float> topRightIsoTilePos =
		    this->screenToTileCoords(topRightIsoScreenPos, 0.0f, TileViewMode::Isometric);
		Vec3<float> bottomLeftIsoTilePos =
		    this->screenToTileCoords(bottomLeftIsoScreenPos, 0.0f, TileViewMode::Isometric);
		Vec3<float> bottomRightIsoTilePos =
		    this->screenToTileCoords(bottomRightIsoScreenPos, 0.0f, TileViewMode::Isometric);

		Vec2<float> topLeftRectPos = this->tileToOffsetScreenCoords(topLeftIsoTilePos);
		Vec2<float> topRightRectPos = this->tileToOffsetScreenCoords(topRightIsoTilePos);
		Vec2<float> bottomLeftRectPos = this->tileToOffsetScreenCoords(bottomLeftIsoTilePos);
		Vec2<float> bottomRightRectPos = this->tileToOffsetScreenCoords(bottomRightIsoTilePos);

		r.drawLine(topLeftRectPos, topRightRectPos, this->strategyViewBoxColour,
		           this->strategyViewBoxThickness);
		r.drawLine(topRightRectPos, bottomRightRectPos, this->strategyViewBoxColour,
		           this->strategyViewBoxThickness);

		r.drawLine(bottomRightRectPos, bottomLeftRectPos, this->strategyViewBoxColour,
		           this->strategyViewBoxThickness);
		r.drawLine(bottomLeftRectPos, topLeftRectPos, this->strategyViewBoxColour,
		           this->strategyViewBoxThickness);
	}
}

void TileView::refreshDisplaySize()
{
	const Vec2<int> next{fw().displayGetWidth(), fw().displayGetHeight()};
	if (next != dpySize)
	{
		dpySize = next;
	}
}

void TileView::update()
{
	refreshDisplaySize();
	// Scrolling is applied per rendered frame (CityTileView/BattleTileView::render), not here:
	// update() runs at the simulation rate, which is neither the display rate nor fixed.
}
}; // namespace OpenApoc
