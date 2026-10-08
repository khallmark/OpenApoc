// Legacy ambient migration must remove known defaults while preserving modified missions.
#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/gamestate.h"
#include "game/state/gametime.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/shared/organisation.h"
#include "tests/test_helpers.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <vector>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;
namespace
{
using Target = Organisation::MissionPattern::Target;
using Relation = Organisation::Relation;
using Mission = Organisation::RecurringMission;
UString commonPath, gamePath;
constexpr uint64_t MINUTE = TICKS_PER_MINUTE;

struct ExpectedMission
{
	UString label, org, city;
	Mission mission;
};

size_t missionCount(const GameState &state)
{
	size_t count = 0;
	for (const auto &org : state.organisations)
		for (const auto &city : org.second->recurring_missions)
			count += city.second.size();
	return count;
}

bool sameMission(const Mission &a, const Mission &b)
{
	const auto &x = a.pattern;
	const auto &y = b.pattern;
	return a.time == b.time && a.maxLiners == b.maxLiners &&
	       x.minIntervalRepeat == y.minIntervalRepeat &&
	       x.maxIntervalRepeat == y.maxIntervalRepeat && x.minAmount == y.minAmount &&
	       x.maxAmount == y.maxAmount && x.allowedTypes == y.allowedTypes && x.target == y.target &&
	       x.relation == y.relation;
}

bool test_loaded_defaults()
{
	auto state = mksp<GameState>();
	TEST_REQUIRE(state->loadGame(commonPath), "could not load common fixture");
	state->ufo_incursions.clear();
	state->ufo_mission_preference.clear();
	state->vehicleParkSpawnTable.clear();
	state->fireHazardPowerTable.clear();
	TEST_REQUIRE(state->loadGame(gamePath), "could not load base fixture");
	state->startGame();
	const auto before = missionCount(*state);
	std::vector<ExpectedMission> expected;
	uint64_t next = 6220800;
	for (auto &org : state->organisations)
		for (auto &city : org.second->recurring_missions)
			for (auto &mission : city.second)
			{
				mission.time = next++; // Saved campaigns reschedule these initial extractor times.
				if (org.first == "ORG_MEGAPOL" ||
				    mission.pattern.allowedTypes.count({state.get(), "VEHICLETYPE_SPACE_LINER"}))
					expected.push_back(
					    {"existing police/liner", org.first, city.first.id, mission});
			}
	TEST_REQUIRE(expected.size() == 5, "fixture must contain three police and two liner defaults");
	state->initState();
	TEST_CHECK(missionCount(*state) == expected.size(),
	           "legacy archive mission count {0} became {1}, expected 5", before,
	           missionCount(*state));
	for (const auto &keep : expected)
	{
		const auto &missions =
		    state->organisations.at(keep.org)->recurring_missions.at({state.get(), keep.city});
		TEST_CHECK(std::any_of(missions.begin(), missions.end(),
		                       [&](const auto &m) { return sameMission(m, keep.mission); }),
		           "existing nonambient default changed: {0}", keep.org);
	}
	std::cout << "Loaded archive recurring rows: " << before << " -> " << missionCount(*state)
	          << '\n';
	return true;
}

void addLegacyDefinitions(GameState &state)
{
	auto add = [&](const UString &org, Mission mission) {
		state.organisations.at(org)->recurring_missions[{&state, "CITYMAP_HUMAN"}].push_back(
		    mission);
	};
	// Exact 27 definitions from base 42c48e7f extract_organisations.cpp. The independently
	// extracted old base archive supplies the same signatures. Scheduled times are arbitrary.
	add("ORG_CULT_OF_SIRIUS",
	    {1234567 + 0,
	     60480,
	     112320,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {}});
	add("ORG_CYBERWEB",
	    {1234567 + 1,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_DIABLO",
	    {1234567 + 2,
	     95040,
	     164160,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {}});
	add("ORG_ENERGEN",
	    {1234567 + 3,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_EVONET",
	    {1234567 + 4,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_EXTROPIANS",
	    {1234567 + 5,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_GENERAL_METRO",
	    {1234567 + 6,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_GOVERNMENT", {1234567 + 7,
	                       25920,
	                       60480,
	                       1,
	                       1,
	                       {{&state, "VEHICLETYPE_RESCUE_TRANSPORT"}},
	                       Target::OwnedOrOther,
	                       {}});
	add("ORG_GOVERNMENT", {1234567 + 8,
	                       25920,
	                       60480,
	                       1,
	                       1,
	                       {{&state, "VEHICLETYPE_CONSTRUCTION_VEHICLE"}},
	                       Target::OwnedOrOther,
	                       {}});
	add("ORG_GOVERNMENT",
	    {1234567 + 9,
	     17280,
	     34560,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {}});
	add("ORG_GRAVBALL_LEAGUE",
	    {1234567 + 10,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_LIFETREE",
	    {1234567 + 11,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_MARSEC",
	    {1234567 + 12,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_MUTANT_ALLIANCE",
	    {1234567 + 13,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_NANOTECH",
	    {1234567 + 14,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_NUTRIVEND",
	    {1234567 + 15,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_OSIRON",
	    {1234567 + 16,
	     95040,
	     164160,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {}});
	add("ORG_PSYKE",
	    {1234567 + 17,
	     95040,
	     164160,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {}});
	add("ORG_SANCTUARY_CLINIC",
	    {1234567 + 18,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_SENSOVISION",
	    {1234567 + 19,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_SOLMINE",
	    {1234567 + 20,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_SUPERDYNAMICS",
	    {1234567 + 21,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_SYNTHEMESH",
	    {1234567 + 22,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_S_E_L_F_",
	    {1234567 + 23,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_TECHNOCRATS",
	    {1234567 + 24,
	     129600,
	     216000,
	     1,
	     1,
	     {{&state, "VEHICLETYPE_BLAZER_TURBO_BIKE"}, {&state, "VEHICLETYPE_CIVILIAN_CAR"}},
	     Target::OwnedOrOther,
	     {Relation::Allied}});
	add("ORG_TRANSTELLAR", {1234567 + 25,
	                        43200,
	                        95040,
	                        1,
	                        1,
	                        {{&state, "VEHICLETYPE_AUTOTRANS"}},
	                        Target::Other,
	                        {Relation::Allied, Relation::Friendly, Relation::Neutral}});
	add("ORG_TRANSTELLAR", {1234567 + 26,
	                        43200,
	                        95040,
	                        1,
	                        3,
	                        {{&state, "VEHICLETYPE_AIRRANS"}},
	                        Target::Other,
	                        {Relation::Allied, Relation::Friendly, Relation::Neutral}});
}

bool test_custom_patterns_survive()
{
	auto state = mksp<GameState>();
	TEST_REQUIRE(loadStartedGameState(*state, commonPath, gamePath), "could not load city fixture");
	const auto baseline = missionCount(*state);
	TEST_REQUIRE(baseline == 5, "fixture retained unexpected legacy rows");
	addLegacyDefinitions(*state);
	TEST_REQUIRE(missionCount(*state) == baseline + 27, "could not inject all legacy definitions");
	const Mission ordinary{
	    7654321,
	    15 * MINUTE,
	    25 * MINUTE,
	    1,
	    1,
	    {{state.get(), "VEHICLETYPE_CIVILIAN_CAR"}, {state.get(), "VEHICLETYPE_BLAZER_TURBO_BIKE"}},
	    Target::OwnedOrOther,
	    {Relation::Allied}};
	std::vector<ExpectedMission> expected;
	auto keep = [&](const UString &label, Mission mission, const UString &org = "ORG_SUPERDYNAMICS",
	                const UString &city = "CITYMAP_HUMAN")
	{
		mission.time = 9000000 + expected.size();
		state->organisations.at(org)->recurring_missions[{state.get(), city}].push_back(mission);
		expected.push_back({label, org, city, mission});
	};
	auto changed = ordinary;
	changed.pattern.target = Target::Other;
	keep("other target", changed);
	changed = ordinary;
	changed.pattern.target = Target::Owned;
	keep("owned target", changed);
	changed = ordinary;
	changed.pattern.minIntervalRepeat++;
	keep("minimum interval", changed);
	changed = ordinary;
	changed.pattern.maxIntervalRepeat--;
	keep("maximum interval", changed);
	changed = ordinary;
	changed.pattern.minAmount = 0;
	keep("minimum count", changed);
	changed = ordinary;
	changed.pattern.maxAmount = 2;
	keep("maximum count", changed);
	changed = ordinary;
	changed.pattern.relation = {Relation::Hostile};
	keep("hostile relation", changed);
	changed = ordinary;
	changed.pattern.allowedTypes = {{state.get(), "VEHICLETYPE_CIVILIAN_CAR"}};
	keep("type subset", changed);
	changed = ordinary;
	changed.pattern.allowedTypes.insert({state.get(), "VEHICLETYPE_CONSTRUCTION_VEHICLE"});
	keep("ambient type superset", changed);
	changed = ordinary;
	changed.pattern.allowedTypes.insert({state.get(), "VEHICLETYPE_POLICE_CAR"});
	keep("nonambient type superset", changed);
	changed = ordinary;
	changed.pattern.allowedTypes.clear();
	keep("empty type set", changed);
	changed = ordinary;
	changed.maxLiners = 17;
	keep("runtime liner limit", changed);
	keep("alien city", ordinary, "ORG_SUPERDYNAMICS", "CITYMAP_ALIEN");
	keep("player organisation", ordinary, "ORG_X-COM");
	changed = {123,
	           3 * MINUTE,
	           7 * MINUTE,
	           1,
	           1,
	           {{state.get(), "VEHICLETYPE_RESCUE_TRANSPORT"}},
	           Target::OwnedOrOther};
	keep("government signature on another organisation", changed);
	changed = {123,
	           5 * MINUTE,
	           11 * MINUTE,
	           1,
	           3,
	           {{state.get(), "VEHICLETYPE_AIRTRANS"}},
	           Target::Other,
	           {Relation::Allied, Relation::Friendly, Relation::Neutral}};
	keep("corrected modded air transport", changed, "ORG_TRANSTELLAR");
	state->initState();
	TEST_CHECK(missionCount(*state) == baseline + expected.size(),
	           "migration removed custom missions or retained legacy rows: {0} vs {1}",
	           missionCount(*state), baseline + expected.size());
	for (const auto &record : expected)
	{
		const auto &missions =
		    state->organisations.at(record.org)->recurring_missions.at({state.get(), record.city});
		TEST_CHECK(std::any_of(missions.begin(), missions.end(),
		                       [&](const auto &m) { return sameMission(m, record.mission); }),
		           "custom pattern lost or changed: {0}", record.label);
	}
	const auto firstCount = missionCount(*state);
	state->initState();
	TEST_CHECK(missionCount(*state) == firstCount,
	           "repeat initialization changed recurring patterns");
	std::cout << "Synthetic legacy rows: 27; custom rows: " << expected.size()
	          << "; expected retained rows: " << baseline + expected.size()
	          << "; actual retained rows: " << firstCount << '\n';
	return true;
}
} // namespace

int main(int argc, char **argv)
{
	config().addPositionalArgument("common", "Common gamestate to load");
	config().addPositionalArgument("gamestate", "Gamestate to load");
	if (config().parseOptions(argc, argv))
		return EXIT_FAILURE;
	applyDeterministicTestConfig();
	commonPath = config().getString("common");
	gamePath = config().getString("gamestate");
	if (commonPath.empty() || gamePath.empty())
		return EXIT_FAILURE;
	Framework fw("OpenApoc", false);
	return runTestSuite({{"loaded_defaults", test_loaded_defaults},
	                     {"custom_patterns_survive", test_custom_patterns_survive}});
}
