#pragma once

#include "framework/stage.h"
#include "library/sp.h"

namespace OpenApoc
{

class GameState;
class Building;
class Form;
class AgentAssignment;

class BuildingScreen : public Stage
{
  private:
	sp<Form> menuform;
	sp<GameState> state;
	sp<Building> building;
	sp<AgentAssignment> agentAssignment;

  public:
	BuildingScreen(sp<GameState> state, sp<Building> building);
	~BuildingScreen() override;
	// Stage control
	// Report how many agents the assignment widget actually has selected. EXTERMINATE refuses

	// outright when that list is empty, and a driver clicking rows by measured pixel offsets

	// has no other way to know whether its clicks landed.

	UString harnessDetail() const override;

	void begin() override;
	void pause() override;
	void resume() override;
	void finish() override;
	void eventOccurred(Event *e) override;
	void update() override;
	void render() override;
	bool isTransition() override;
};
}; // namespace OpenApoc
