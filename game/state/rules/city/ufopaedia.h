#pragma once

#include "game/state/city/research.h"
#include "game/state/stateobject.h"
#include "library/sp.h"
#include "library/strings.h"
#include <list>

namespace OpenApoc
{

class ResearchTopic;
class LazyImage;
class GameState;

class UfopaediaEntry : public StateObject<UfopaediaEntry>
{
  public:
	enum class Data
	{
		Nothing,
		Organisation,
		Vehicle,
		VehicleEquipment,
		Equipment,
		Facility,
		Building
	};
	UfopaediaEntry();
	UString title;
	UString description;
	sp<LazyImage> background;
	// The ID of the 'dynamic' data shown with this entry (income/balance for organisations, stats
	// for weapons etc.)
	UString data_id;
	Data data_type;
	ResearchDependency dependency;
	// UFO2P DAT_001302c6 @ file 0x19196A, applied after patch via unique PCX.
	bool startVisible = false;
	bool startVisibleFromExe = false;
	// Catalog packed category / entry. 0xFF / 0xFFFF = not bound.
	unsigned catalogCategory = 0xFF;
	unsigned catalogIndex = 0xFFFF;
	// FUN_0008c860 @ file 0xEEF04: case 2 gates DAT_00183b3b, case 3 DAT_00183b0a.
	bool isVisible(const GameState &state) const;
};

class UfopaediaCategory : public StateObject<UfopaediaCategory>
{
  public:
	UString title;
	UString description;
	sp<LazyImage> background;
	// Ordered list of entry references. Entries themselves are owned by
	// GameState::ufopaedia_entries; this list just tracks category membership
	// and display order.
	std::list<StateRef<UfopaediaEntry>> entries;
};

} // namespace OpenApoc
