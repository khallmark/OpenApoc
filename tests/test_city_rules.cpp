#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/city/agentmission.h"
#include "game/state/city/base.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/agenttype.h"
#include "game/state/rules/city/ufoincursion.h"
#include "game/state/rules/city/ufomissionpreference.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/organisation.h"
#include "library/sp.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static sp<GameState> g_state;

static int countOwnedType(GameState &state, const UString &orgId, StateRef<VehicleType> type)
{
	int count = 0;
	for (auto &v : state.vehicles)
	{
		if (v.second->owner.id == orgId && v.second->type == type)
		{
			count++;
		}
	}
	return count;
}

static bool isSpaceLiner(GameState &state, Organisation &org, StateRef<VehicleType> type)
{
	for (auto &m : org.recurring_missions[{&state, "CITYMAP_HUMAN"}])
	{
		if ((m.pattern.target == Organisation::MissionPattern::Target::ArriveFromSpace ||
		     m.pattern.target == Organisation::MissionPattern::Target::DepartToSpace) &&
		    m.pattern.allowedTypes.find(type) != m.pattern.allowedTypes.end())
		{
			return true;
		}
	}
	return false;
}

static bool test_org_park_funds()
{
	auto &state = *g_state;
	Organisation *org = nullptr;
	StateRef<VehicleType> type;
	for (auto &orgPair : state.organisations)
	{
		bool hasHuman = false;
		for (auto &b : orgPair.second->buildings)
		{
			if (b->city.id == "CITYMAP_HUMAN")
			{
				hasHuman = true;
				break;
			}
		}
		if (!hasHuman)
		{
			continue;
		}
		for (auto &entry : orgPair.second->vehiclePark)
		{
			if (isSpaceLiner(state, *orgPair.second, entry.first))
			{
				continue;
			}
			org = orgPair.second.get();
			type = entry.first;
			break;
		}
		if (org)
		{
			break;
		}
	}
	TEST_REQUIRE(org != nullptr, "no organisation with a non-liner vehiclePark and human building");
	TEST_REQUIRE(!!type, "vehicle park type missing");

	state.economy[type.id].currentPrice = 1000;
	const int already = countOwnedType(state, org->id, type);
	org->vehiclePark[type] = already + 2;
	org->balance = 0;

	org->updateVehicleAgentPark(state);
	TEST_REQUIRE(countOwnedType(state, org->id, type) == already,
	             "zero balance still spawned a vehicle");

	org->balance = 1000;
	org->updateVehicleAgentPark(state);
	TEST_REQUIRE(countOwnedType(state, org->id, type) == already + 1,
	             "funded park spawn did not add one vehicle");
	TEST_REQUIRE(org->balance == 0, "balance after one 1000-credit spawn is {0}", org->balance);
	return true;
}

static bool test_purchase_deduct()
{
	auto &state = *g_state;
	TEST_REQUIRE(state.current_base && state.current_base->building,
	             "player base/building missing");
	StateRef<Building> buyer = state.current_base->building;
	TEST_REQUIRE(buyer->owner == state.getPlayer(), "buyer is not the player");

	Organisation *seller = nullptr;
	for (auto &orgPair : state.organisations)
	{
		if (orgPair.second->id == state.getPlayer().id)
		{
			continue;
		}
		if (orgPair.second->getPurchaseBuilding(state, buyer))
		{
			seller = orgPair.second.get();
			break;
		}
	}
	TEST_REQUIRE(seller != nullptr, "no seller with a different alive building in the buyer city");

	StateRef<AEquipmentType> item;
	for (auto &eq : state.agent_equipment)
	{
		auto econ = state.economy.find(eq.first);
		if (econ != state.economy.end() && econ->second.currentPrice > 0)
		{
			item = {&state, eq.first};
			break;
		}
	}
	TEST_REQUIRE(!!item, "no agent equipment with a positive economy price");

	const int price = state.economy[item.id].currentPrice;
	const int stockBefore = state.economy[item.id].currentStock;
	const int balanceBefore = buyer->owner->balance;
	const int sellerBefore = seller->balance;
	TEST_REQUIRE(balanceBefore >= price, "player cannot afford one unit ({0} < {1})", balanceBefore,
	             price);

	size_t cargoBefore = 0;
	for (auto &b : seller->buildings)
	{
		cargoBefore += b->cargo.size();
	}

	seller->purchase(state, buyer, item, 1);
	TEST_REQUIRE(buyer->owner->balance == balanceBefore - price, "buyer balance {0}, expected {1}",
	             buyer->owner->balance, balanceBefore - price);
	TEST_REQUIRE(seller->balance == sellerBefore + price, "seller balance {0}, expected {1}",
	             seller->balance, sellerBefore + price);
	TEST_REQUIRE(state.economy[item.id].currentStock == stockBefore - 1, "stock {0}, expected {1}",
	             state.economy[item.id].currentStock, stockBefore - 1);
	size_t cargoAfter = 0;
	for (auto &b : seller->buildings)
	{
		cargoAfter += b->cargo.size();
	}
	TEST_REQUIRE(cargoAfter == cargoBefore + 1, "seller cargo {0} -> {1}, expected +1",
	             (unsigned)cargoBefore, (unsigned)cargoAfter);
	return true;
}

static bool test_goto_building_fallback()
{
	auto &state = *g_state;
	sp<Agent> agent;
	for (auto &a : state.agents)
	{
		if (a.second->owner == state.getPlayer() && a.second->type &&
		    a.second->type->role == AgentType::Role::Soldier && a.second->city)
		{
			agent = a.second;
			break;
		}
	}
	TEST_REQUIRE(agent != nullptr, "no player soldier with a city");

	StateRef<Building> target;
	for (auto &b : agent->city->buildings)
	{
		if (b != agent->currentBuilding && b != agent->homeBuilding && b->isAlive())
		{
			target = b;
			break;
		}
	}
	TEST_REQUIRE(!!target, "no alternate target building");

	target->crewQuarters = {10000, 10000, 10000};

	AgentMission mission;
	mission.type = AgentMission::MissionType::GotoBuilding;
	mission.targetBuilding = target;
	mission.allowTeleporter = false;
	mission.allowTaxi = false;
	mission.start(state, *agent);

	TEST_REQUIRE(!!agent->currentBuilding, "fallback did not enter a building");
	TEST_REQUIRE(agent->currentBuilding != target,
	             "fallback entered the unreachable target building");
	return true;
}

static bool test_overspawn_invasion()
{
	auto &state = *g_state;
	TEST_REQUIRE(state.current_city.id == "CITYMAP_HUMAN", "current_city is {0}",
	             state.current_city.id);
	TEST_REQUIRE(state.cities.find("CITYMAP_ALIEN") != state.cities.end(), "CITYMAP_ALIEN missing");
	TEST_REQUIRE(state.vehicle_types.find("VEHICLETYPE_ALIEN_MOTHERSHIP") !=
	                 state.vehicle_types.end(),
	             "VEHICLETYPE_ALIEN_MOTHERSHIP missing");
	TEST_REQUIRE(state.organisations.find("ORG_ALIEN") != state.organisations.end(),
	             "ORG_ALIEN missing");

	for (auto &pref : state.ufo_mission_preference)
	{
		pref.second->missionList = {UFOIncursion::PrimaryMission::Overspawn};
	}

	auto alienCity = state.cities["CITYMAP_ALIEN"];
	TEST_REQUIRE(alienCity && alienCity->size.z > 0, "alien city has no z size");
	const Vec3<float> pos = {40.0f, 40.0f, static_cast<float>(alienCity->size.z - 1)};
	auto mothership = alienCity->placeVehicle(state, {&state, "VEHICLETYPE_ALIEN_MOTHERSHIP"},
	                                          {&state, "ORG_ALIEN"}, pos, 0.0f);
	TEST_REQUIRE(mothership != nullptr, "placeVehicle mothership failed");
	TEST_REQUIRE(mothership->city.id == "CITYMAP_ALIEN", "mothership city is {0}",
	             mothership->city.id);

	state.invasion();

	TEST_REQUIRE(mothership->city.id != "CITYMAP_ALIEN",
	             "mothership never left CITYMAP_ALIEN (city={0})", mothership->city.id);
	TEST_REQUIRE(!mothership->missions.empty(), "mothership has no missions after invasion");
	bool sawInfiltrate = false;
	bool sawAttackBuilding = false;
	for (auto &m : mothership->missions)
	{
		if (m.type == VehicleMission::MissionType::InfiltrateSubvert && m.subvert == false)
		{
			sawInfiltrate = true;
		}
		if (m.type == VehicleMission::MissionType::AttackBuilding)
		{
			sawAttackBuilding = true;
		}
	}
	TEST_REQUIRE(sawInfiltrate,
	             "Overspawn mothership has no InfiltrateSubvert (subvert=false); "
	             "front={0} back={1}",
	             (int)mothership->missions.front().type, (int)mothership->missions.back().type);
	TEST_REQUIRE(!sawAttackBuilding, "Overspawn mothership was given AttackBuilding");
	return true;
}

int main(int argc, char **argv)
{
	config().addPositionalArgument("common", "Common gamestate to load");
	config().addPositionalArgument("gamestate", "Gamestate to load");
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	applyDeterministicTestConfig();

	const auto common = config().getString("common");
	const auto gamestate = config().getString("gamestate");
	if (common.empty() || gamestate.empty())
	{
		LogError("Must provide common and gamestate paths");
		return EXIT_FAILURE;
	}

	Framework fw("OpenApoc", false);
	g_state = mksp<GameState>();
	if (!loadStartedGameState(*g_state, common, gamestate))
	{
		return EXIT_FAILURE;
	}

	return runTestSuite({
	    {"org_park_funds", test_org_park_funds},
	    {"purchase_deduct", test_purchase_deduct},
	    {"goto_building_fallback", test_goto_building_fallback},
	    {"overspawn_invasion", test_overspawn_invasion},
	});
}
