#include "framework/configfile.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_factories()
{
	GameState state;
	Vehicle v;

	auto infiltrate = VehicleMission::infiltrateOrSubvertBuilding(state, v, false);
	TEST_REQUIRE(infiltrate.type == VehicleMission::MissionType::InfiltrateSubvert,
	             "infiltrate type");
	TEST_REQUIRE(infiltrate.subvert == false, "infiltrate subvert flag");

	auto subvert = VehicleMission::infiltrateOrSubvertBuilding(state, v, true);
	TEST_REQUIRE(subvert.type == VehicleMission::MissionType::InfiltrateSubvert, "subvert type");
	TEST_REQUIRE(subvert.subvert == true, "subvert flag");

	auto recover = VehicleMission::recoverVehicle(state, v, {});
	TEST_REQUIRE(recover.type == VehicleMission::MissionType::RecoverVehicle, "recover type");

	auto teleport = VehicleMission::teleport(state, v, {1, 2, 3});
	TEST_REQUIRE(teleport.type == VehicleMission::MissionType::Teleport, "teleport type");
	TEST_REQUIRE(teleport.targetLocation == Vec3<int>(1, 2, 3), "teleport target");

	auto snooze = VehicleMission::snooze(state, v, 50);
	TEST_REQUIRE(snooze.type == VehicleMission::MissionType::Snooze, "snooze type");
	TEST_REQUIRE(snooze.timeToSnooze == 50, "snooze ticks");

	auto attackB = VehicleMission::attackBuilding(state, v, {});
	TEST_REQUIRE(attackB.type == VehicleMission::MissionType::AttackBuilding, "attackBuilding type");
	return true;
}

static bool test_noop_get_next_destination()
{
	GameState state;
	Vehicle v;
	Vec3<float> dest{9, 9, 9};
	float facing = 7.0f;
	int turbo = 11;

	VehicleMission teleport;
	teleport.type = VehicleMission::MissionType::Teleport;
	TEST_REQUIRE(!teleport.getNextDestination(state, v, dest, facing, turbo),
	             "Teleport getNextDestination should return false");
	TEST_REQUIRE(dest == Vec3<float>(9, 9, 9), "Teleport wrote dest");
	TEST_REQUIRE(facing == 7.0f, "Teleport wrote facing");
	TEST_REQUIRE(turbo == 11, "Teleport wrote turbo");

	VehicleMission recover;
	recover.type = VehicleMission::MissionType::RecoverVehicle;
	TEST_REQUIRE(!recover.getNextDestination(state, v, dest, facing, turbo),
	             "RecoverVehicle getNextDestination should return false");
	TEST_REQUIRE(dest == Vec3<float>(9, 9, 9), "RecoverVehicle wrote dest");
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
