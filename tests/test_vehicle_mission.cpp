#include "framework/configfile.h"
#include "game/state/city/base.h"
#include "game/state/city/building.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/shared/organisation.h"
#include "tests/test_helpers.h"
#include <array>

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
	TEST_REQUIRE(attackB.type == VehicleMission::MissionType::AttackBuilding,
	             "attackBuilding type");
	return true;
}

static bool test_subversion_completion_exposes_base_without_takeover()
{
	GameState state;
	auto victim = mksp<Organisation>();
	victim->infiltrationValue = 37;
	state.organisations["ORG_TEST_VICTIM"] = victim;

	auto target = mksp<Building>();
	target->owner = {&state, "ORG_TEST_VICTIM"};
	state.buildings["BUILDING_TEST_TARGET"] = target;
	auto base = mksp<Base>();
	base->building = {&state, "BUILDING_TEST_TARGET"};
	state.player_bases["BASE_TEST_TARGET"] = base;
	target->base = {&state, "BASE_TEST_TARGET"};

	auto vehicleType = mksp<VehicleType>();
	state.vehicle_types["VEHICLETYPE_TEST_SUBVERSION"] = vehicleType;
	Vehicle vehicle;
	vehicle.type = {&state, "VEHICLETYPE_TEST_SUBVERSION"};

	auto mission = VehicleMission::infiltrateOrSubvertBuilding(state, vehicle, true,
	                                                           {&state, "BUILDING_TEST_TARGET"});
	mission.missionCounter = 1;
	mission.start(state, vehicle);

	TEST_REQUIRE(base->knownToAliens, "subversion completion did not expose target base");
	TEST_REQUIRE(!victim->takenOver && !victim->militarized && victim->infiltrationValue == 37,
	             "subversion completion changed organisation takeover state");
	return true;
}

static bool test_ground_footprint_tiles()
{
	auto one = GroundVehicleTileHelper::footprintTiles({5, 6, 1}, {1, 1});
	TEST_REQUIRE(one.size() == 1, "1x1 count");
	TEST_REQUIRE(one[0] == Vec3<int>(5, 6, 1), "1x1 origin");

	auto wide = GroundVehicleTileHelper::footprintTiles({10, 20, 2}, {2, 1});
	TEST_REQUIRE(wide.size() == 2, "2x1 count");
	TEST_REQUIRE(wide[0] == Vec3<int>(10, 20, 2), "2x1 first");
	TEST_REQUIRE(wide[1] == Vec3<int>(11, 20, 2), "2x1 second");

	auto zero = GroundVehicleTileHelper::footprintTiles({0, 0, 0}, {0, 0});
	TEST_REQUIRE(zero.size() == 1, "zero size still occupies origin");
	return true;
}

static bool test_alien_exposure_threshold()
{
	// UFO2P uses inclusive rand16(100) < moved_count * 5.
	TEST_REQUIRE(Base::alienExposureRollSucceeds(4, 1), "one alien roll 4");
	TEST_REQUIRE(!Base::alienExposureRollSucceeds(5, 1), "one alien roll 5");
	TEST_REQUIRE(Base::alienExposureRollSucceeds(99, 20), "twenty aliens roll 99");
	TEST_REQUIRE(!Base::alienExposureRollSucceeds(100, 20), "twenty aliens roll 100");
	TEST_REQUIRE(!Base::alienExposureRollSucceeds(0, 0), "zero aliens never exposes");
	return true;
}

static bool test_known_base_slot_selection()
{
	std::array<bool, GameState::UFO2P_BASE_SLOT_COUNT> active{};
	std::array<bool, GameState::UFO2P_BASE_SLOT_COUNT> known{};
	active[2] = true;
	known[2] = true;
	active[14] = true;
	known[14] = true;
	known[1] = true; // Inactive slots must be ignored.

	TEST_REQUIRE(GameState::selectKnownBaseSlot(active, known, 3) == 14, "cyclic scan from slot 3");
	TEST_REQUIRE(GameState::selectKnownBaseSlot(active, known, 15) == 2,
	             "cyclic scan wraps at slot 15");
	TEST_REQUIRE(GameState::selectKnownBaseSlot(active, known, 14) == 14,
	             "start slot is considered first");
	active[2] = false;
	active[14] = false;
	TEST_REQUIRE(GameState::selectKnownBaseSlot(active, known, 0) == -1, "no active known base");
	return true;
}

static bool test_select_dimension_exit_portal()
{
	TEST_REQUIRE(Vehicle::selectDimensionExitPortal(0, 3) == 0, "index 0 of 3");
	TEST_REQUIRE(Vehicle::selectDimensionExitPortal(2, 3) == 2, "index 2 of 3");
	TEST_REQUIRE(Vehicle::selectDimensionExitPortal(3, 3) == -1, "index 3 of 3 is out of range");
	TEST_REQUIRE(Vehicle::selectDimensionExitPortal(-1, 3) == -1, "unset index");
	TEST_REQUIRE(Vehicle::selectDimensionExitPortal(0, 0) == -1, "empty portal list");
	TEST_REQUIRE(Vehicle::selectDimensionExitPortal(1, 1) == -1, "index 1 of 1 is out of range");
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
	    {"subversion_completion_exposes_base_without_takeover",
	     test_subversion_completion_exposes_base_without_takeover},
	    {"ground_footprint_tiles", test_ground_footprint_tiles},
	    {"alien_exposure_threshold", test_alien_exposure_threshold},
	    {"known_base_slot_selection", test_known_base_slot_selection},
	    {"select_dimension_exit_portal", test_select_dimension_exit_portal},
	    {"noop_get_next_destination", test_noop_get_next_destination},
	});
}
