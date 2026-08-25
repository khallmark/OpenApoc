#include "framework/configfile.h"
#include "game/state/city/agentmission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/agenttype.h"
#include "game/state/shared/agent.h"
#include "library/sp.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static Agent makeSoldier(GameState &state)
{
	auto type = mksp<AgentType>();
	type->role = AgentType::Role::Soldier;
	state.agent_types["AGENTTYPE_TEST"] = type;
	Agent a;
	a.type = {&state, "AGENTTYPE_TEST"};
	return a;
}

static bool test_factories()
{
	GameState state;
	Agent a = makeSoldier(state);

	auto gotoB = AgentMission::gotoBuilding(state, a);
	TEST_REQUIRE(gotoB.type == AgentMission::MissionType::GotoBuilding, "gotoBuilding type");

	auto snooze = AgentMission::snooze(state, a, 123);
	TEST_REQUIRE(snooze.type == AgentMission::MissionType::Snooze, "snooze type");
	TEST_REQUIRE(snooze.timeToSnooze == 123, "snooze ticks");

	auto restart = AgentMission::restartNextMission(state, a);
	TEST_REQUIRE(restart.type == AgentMission::MissionType::RestartNextMission, "restart type");

	auto pickup = AgentMission::awaitPickup(state, a, {});
	TEST_REQUIRE(pickup.type == AgentMission::MissionType::AwaitPickup, "awaitPickup type");

	auto teleport = AgentMission::teleport(state, a, {});
	TEST_REQUIRE(teleport.type == AgentMission::MissionType::Teleport, "teleport type");

	auto investigate = AgentMission::investigateBuilding(state, a, {});
	TEST_REQUIRE(investigate.type == AgentMission::MissionType::InvestigateBuilding,
	             "investigate type");
	return true;
}

static bool test_noop_get_next_destination()
{
	GameState state;
	Agent a = makeSoldier(state);
	Vec3<float> dest{9, 9, 9};

	const AgentMission::MissionType noop[] = {
	    AgentMission::MissionType::Teleport, AgentMission::MissionType::Snooze,
	    AgentMission::MissionType::AwaitPickup, AgentMission::MissionType::RestartNextMission};
	for (auto type : noop)
	{
		AgentMission mission;
		mission.type = type;
		dest = {9, 9, 9};
		TEST_REQUIRE(!mission.getNextDestination(state, a, dest),
		             "mission type {0} should return false", (int)type);
		TEST_REQUIRE(dest == Vec3<float>(9, 9, 9), "mission type {0} wrote dest {1}", (int)type,
		             dest);
	}
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	applyDeterministicTestConfig();
	return runTestSuite({
	    {"factories", test_factories},
	    {"noop_get_next_destination", test_noop_get_next_destination},
	});
}
