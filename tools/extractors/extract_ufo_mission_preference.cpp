#include "framework/logger.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/ufomissionpreference.h"
#include "library/strings_format.h"
#include "tools/extractors/common/ufo2p.h"
#include "tools/extractors/extractors.h"

namespace OpenApoc
{

static bool patternIdToMission(uint16_t id, UFOIncursion::PrimaryMission &out)
{
	switch (id)
	{
		case UFO_MISSION_PATTERN_ATTACK:
			out = UFOIncursion::PrimaryMission::Attack;
			return true;
		case UFO_MISSION_PATTERN_SUBVERSION:
			out = UFOIncursion::PrimaryMission::Subversion;
			return true;
		case UFO_MISSION_PATTERN_INFILTRATION:
			out = UFOIncursion::PrimaryMission::Infiltration;
			return true;
		case UFO_MISSION_PATTERN_OVERSPAWN:
			out = UFOIncursion::PrimaryMission::Overspawn;
			return true;
		default:
			return false;
	}
}

void InitialGameStateExtractor::extractUfoMissionPreference(GameState &state) const
{
	auto &data = this->ufo2p;
	if (!data.ufo_mission_patterns || data.ufo_mission_patterns->count() < 1)
	{
		LogError("UFO_mission_patterns table missing");
		return;
	}
	const auto table = data.ufo_mission_patterns->get(0);

	// Do not XInclude a copy under common_patch after extract-data — loadGame
	// appends missionList.
	state.ufo_mission_preference.clear();

	auto fill = [&](const UString &id, int week, const uint16_t *slots)
	{
		auto pref = mksp<UFOMissionPreference>();
		pref->week = week;
		for (int i = 0; i < UFO_MISSION_PATTERN_SLOT_COUNT; i++)
		{
			UFOIncursion::PrimaryMission mission;
			if (!patternIdToMission(slots[i], mission))
			{
				LogError("UFO_mission_patterns {0} slot {1} id {2}", id, i, slots[i]);
				continue;
			}
			pref->missionList.push_back(mission);
		}
		state.ufo_mission_preference[id] = pref;
	};

	for (int week = 0; week < UFO_MISSION_PATTERN_WEEK_COUNT; week++)
	{
		fill(format("UFO_MISSION_PREFERENCE_{0}", week + 1), week + 1, table.weekly[week]);
	}
	fill("UFO_MISSION_PREFERENCE_DEFAULT", 0, table.default_list);
}

} // namespace OpenApoc
