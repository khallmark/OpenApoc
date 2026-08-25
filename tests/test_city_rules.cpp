#include "framework/configfile.h"
#include "framework/data.h"
#include "framework/framework.h"
#include "game/state/city/agentmission.h"
#include "game/state/city/base.h"
#include "game/state/city/facility.h"
#include "game/state/rules/city/facilitytype.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/research.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/agenttype.h"
#include "game/state/rules/battle/battlemap.h"
#include "game/state/rules/battle/battlemapparttype.h"
#include "game/state/rules/battle/battlemaptileset.h"
#include "game/state/rules/city/ufogrowth.h"
#include "game/state/rules/city/ufoincursion.h"
#include "game/state/rules/city/ufomissionpreference.h"
#include "game/state/rules/city/ufopaedia.h"
#include "game/state/rules/city/vammotype.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/rules/city/vequipmenttype.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/doodad.h"
#include "game/state/shared/organisation.h"
#include "library/rect.h"
#include "library/sp.h"
#include "tests/test_helpers.h"
#include "tools/extractors/common/agent.h"
#include "tools/extractors/common/building.h"
#include "tools/extractors/common/exe_slide.h"
#include "tools/extractors/common/research.h"
#include <boost/crc.hpp>
#include <list>
#include <map>
#include <vector>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static sp<GameState> g_state;

static int countOwnedType(GameState &state, const UString &orgId, StateRef<VehicleType> type)
{
	int count = 0;
	for (auto &v : state.vehicles)
	{
		if (v.second->owner.id == orgId && v.second->type == type && !v.second->isDead())
		{
			count++;
		}
	}
	return count;
}

static bool isSpaceLiner(GameState &state, const Organisation &org, StateRef<VehicleType> type)
{
	const auto it = org.recurring_missions.find({&state, "CITYMAP_HUMAN"});
	if (it == org.recurring_missions.end())
	{
		return false;
	}
	for (auto &m : it->second)
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

static bool test_cargo_expiry_refund()
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
	const int buyerBefore = buyer->owner->balance;
	const int sellerBefore = seller->balance;
	TEST_REQUIRE(buyerBefore >= price, "player cannot afford one unit ({0} < {1})", buyerBefore,
	             price);

	seller->purchase(state, buyer, item, 1);

	Cargo *paid = nullptr;
	StateRef<Building> cargoBuilding;
	for (auto &b : seller->buildings)
	{
		for (auto &c : b->cargo)
		{
			if (c.cost > 0 && c.destination == buyer && c.id == item.id)
			{
				paid = &c;
				cargoBuilding = b;
				break;
			}
		}
		if (paid)
		{
			break;
		}
	}
	TEST_REQUIRE(paid != nullptr, "purchase did not leave paid cargo on a seller building");

	paid->expirationDate = state.gameTime.getTicks() - 1;
	paid->checkExpiryDate(state, cargoBuilding);

	TEST_REQUIRE(buyer->owner->balance == buyerBefore, "buyer balance {0}, expected refund to {1}",
	             buyer->owner->balance, buyerBefore);
	TEST_REQUIRE(seller->balance == sellerBefore + price,
	             "seller balance {0}, expected to keep purchase credit {1}", seller->balance,
	             sellerBefore + price);
	TEST_REQUIRE(state.economy[item.id].currentStock == stockBefore, "stock {0}, expected {1}",
	             state.economy[item.id].currentStock, stockBefore);
	return true;
}

static bool test_org_park_sell_surplus()
{
	auto &state = *g_state;
	Organisation *org = nullptr;
	StateRef<VehicleType> type;
	int already = 0;
	for (auto &v : state.vehicles)
	{
		if (!v.second || v.second->isDead() || v.second->crashed || !v.second->missions.empty() ||
		    !v.second->currentAgents.empty() || !v.second->owner || !v.second->type)
		{
			continue;
		}
		auto orgIt = state.organisations.find(v.second->owner.id);
		if (orgIt == state.organisations.end() || !orgIt->second)
		{
			continue;
		}
		Organisation *candidate = orgIt->second.get();
		if (!candidate || isSpaceLiner(state, *candidate, v.second->type))
		{
			continue;
		}
		bool hasHuman = false;
		for (auto &b : candidate->buildings)
		{
			if (b->city.id == "CITYMAP_HUMAN")
			{
				hasHuman = true;
				break;
			}
		}
		if (!hasHuman ||
		    candidate->vehiclePark.find(v.second->type) == candidate->vehiclePark.end())
		{
			continue;
		}
		already = countOwnedType(state, candidate->id, v.second->type);
		if (already <= 0)
		{
			continue;
		}
		org = candidate;
		type = v.second->type;
		break;
	}
	TEST_REQUIRE(org != nullptr, "no organisation with an idle parked non-liner vehicle");
	TEST_REQUIRE(!!type, "vehicle park type missing");

	state.economy[type.id].currentPrice = 1000;
	for (auto &entry : org->vehiclePark)
	{
		entry.second = countOwnedType(state, org->id, entry.first);
	}
	org->vehiclePark[type] = already - 1;
	const int balanceBefore = org->balance;

	org->updateVehicleAgentPark(state);
	state.cleanUpDeathNote();
	TEST_REQUIRE(countOwnedType(state, org->id, type) == already - 1,
	             "surplus park vehicle was not sold");
	TEST_REQUIRE(org->balance == balanceBefore + 1000, "surplus sell credited {0}, expected {1}",
	             org->balance, balanceBefore + 1000);
	return true;
}

static bool test_infiltration_display_percent()
{
	TEST_REQUIRE(Organisation::infiltrationDisplayPercent(0) == 0, "0 -> 0");
	TEST_REQUIRE(Organisation::infiltrationDisplayPercent(50) == 25, "50 -> 25");
	TEST_REQUIRE(Organisation::infiltrationDisplayPercent(200) == 100, "200 -> 100");
	TEST_REQUIRE(Organisation::infiltrationDisplayPercent(201) == 100, "201 clamps to 100");
	return true;
}

static int countPref(const std::list<UFOIncursion::PrimaryMission> &list,
                     UFOIncursion::PrimaryMission mission)
{
	int n = 0;
	for (auto &m : list)
	{
		if (m == mission)
		{
			n++;
		}
	}
	return n;
}

static bool test_ufo_mission_preference_loaded()
{
	auto &state = *g_state;
	TEST_REQUIRE(!state.ufo_mission_preference.empty(),
	             "ufo_mission_preference empty (re-extract common_gamestate)");

	// UFO2P non-4 0x155164: 20×10 uint16. 3=Inf 1=Atk 2=Sub 5=Over. Last row DEFAULT.
	auto defIt = state.ufo_mission_preference.find("UFO_MISSION_PREFERENCE_DEFAULT");
	TEST_REQUIRE(defIt != state.ufo_mission_preference.end() && defIt->second, "DEFAULT missing");
	const auto &def = *defIt->second;
	TEST_REQUIRE(def.missionList.size() == 10, "DEFAULT slots {0}", def.missionList.size());
	auto d = def.missionList.begin();
	TEST_REQUIRE(*d++ == UFOIncursion::PrimaryMission::Infiltration, "DEFAULT[0]");
	TEST_REQUIRE(*d++ == UFOIncursion::PrimaryMission::Attack, "DEFAULT[1]");
	TEST_REQUIRE(*d++ == UFOIncursion::PrimaryMission::Attack, "DEFAULT[2]");
	TEST_REQUIRE(*d++ == UFOIncursion::PrimaryMission::Subversion, "DEFAULT[3]");
	TEST_REQUIRE(*d++ == UFOIncursion::PrimaryMission::Subversion, "DEFAULT[4]");
	TEST_REQUIRE(*d++ == UFOIncursion::PrimaryMission::Subversion, "DEFAULT[5]");
	TEST_REQUIRE(*d++ == UFOIncursion::PrimaryMission::Subversion, "DEFAULT[6]");
	TEST_REQUIRE(*d++ == UFOIncursion::PrimaryMission::Overspawn, "DEFAULT[7]");
	TEST_REQUIRE(*d++ == UFOIncursion::PrimaryMission::Overspawn, "DEFAULT[8]");
	TEST_REQUIRE(*d++ == UFOIncursion::PrimaryMission::Overspawn, "DEFAULT[9]");

	auto w1It = state.ufo_mission_preference.find("UFO_MISSION_PREFERENCE_1");
	TEST_REQUIRE(w1It != state.ufo_mission_preference.end() && w1It->second, "week1 missing");
	TEST_REQUIRE(w1It->second->missionList.size() == 10, "week1 slots {0}",
	             w1It->second->missionList.size());
	TEST_REQUIRE(countPref(w1It->second->missionList, UFOIncursion::PrimaryMission::Infiltration) ==
	                 10,
	             "week1 all infiltration");

	auto w4It = state.ufo_mission_preference.find("UFO_MISSION_PREFERENCE_4");
	TEST_REQUIRE(w4It != state.ufo_mission_preference.end() && w4It->second, "week4 missing");
	TEST_REQUIRE(countPref(w4It->second->missionList, UFOIncursion::PrimaryMission::Infiltration) ==
	                 9,
	             "week4 infiltration");
	TEST_REQUIRE(countPref(w4It->second->missionList, UFOIncursion::PrimaryMission::Attack) == 1,
	             "week4 attack");

	auto w13It = state.ufo_mission_preference.find("UFO_MISSION_PREFERENCE_13");
	TEST_REQUIRE(w13It != state.ufo_mission_preference.end() && w13It->second, "week13 missing");
	TEST_REQUIRE(w13It->second->missionList.size() == 10, "week13 slots {0}",
	             w13It->second->missionList.size());
	TEST_REQUIRE(
	    countPref(w13It->second->missionList, UFOIncursion::PrimaryMission::Infiltration) == 3,
	    "week13 infiltration");
	TEST_REQUIRE(countPref(w13It->second->missionList, UFOIncursion::PrimaryMission::Attack) == 3,
	             "week13 attack");
	TEST_REQUIRE(countPref(w13It->second->missionList, UFOIncursion::PrimaryMission::Subversion) ==
	                 2,
	             "week13 subversion");
	TEST_REQUIRE(countPref(w13It->second->missionList, UFOIncursion::PrimaryMission::Overspawn) ==
	                 2,
	             "week13 overspawn");

	// Week 7 is the first Overspawn slot (EXE 3×7,1,2,5).
	auto w7It = state.ufo_mission_preference.find("UFO_MISSION_PREFERENCE_7");
	TEST_REQUIRE(w7It != state.ufo_mission_preference.end() && w7It->second, "week7 missing");
	TEST_REQUIRE(countPref(w7It->second->missionList, UFOIncursion::PrimaryMission::Overspawn) == 1,
	             "week7 overspawn");
	TEST_REQUIRE(countPref(w7It->second->missionList, UFOIncursion::PrimaryMission::Attack) == 1,
	             "week7 attack");
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

static bool test_destination_gate()
{
	auto &state = *g_state;
	auto destCity = state.cities["CITYMAP_ALIEN"];
	TEST_REQUIRE(destCity != nullptr, "CITYMAP_ALIEN missing");
	if (destCity->portals.empty())
	{
		destCity->generatePortals(state);
	}
	TEST_REQUIRE(destCity->portals.size() >= 2, "need two dest portals, have {0}",
	             destCity->portals.size());

	TEST_REQUIRE(destCity->getNearestPortalIndex(destCity->portals[0]->getPosition()) == 0,
	             "nearest portal 0");
	TEST_REQUIRE(destCity->getNearestPortalIndex(destCity->portals[1]->getPosition()) == 1,
	             "nearest portal 1");

	TEST_REQUIRE(state.vehicle_types.find("VEHICLETYPE_PHOENIX_HOVERCAR") !=
	                 state.vehicle_types.end(),
	             "VEHICLETYPE_PHOENIX_HOVERCAR missing");
	TEST_REQUIRE(state.getPlayer(), "no player org");

	const Vec3<float> spawn = {40.0f, 40.0f, static_cast<float>(destCity->size.z - 1)};
	auto v = destCity->placeVehicle(state, {&state, "VEHICLETYPE_PHOENIX_HOVERCAR"},
	                                state.getPlayer(), spawn, 0.0f);
	TEST_REQUIRE(v != nullptr, "placeVehicle phoenix failed");

	auto order =
	    VehicleMission::gotoPortal(state, *v, (Vec3<int>)destCity->portals[1]->getPosition());
	TEST_REQUIRE(order.type == VehicleMission::MissionType::GotoPortal, "gotoPortal type");
	TEST_REQUIRE(v->destinationPortalIndex == 1, "gotoPortal stored dest index {0}",
	             v->destinationPortalIndex);

	v->removeFromMap(state);
	v->currentBuilding.clear();
	v->betweenDimensions = true;
	v->destinationPortalIndex = 1;
	const auto expected = destCity->portals[1]->getPosition();
	v->leaveDimensionGate(state);

	TEST_REQUIRE(!v->betweenDimensions, "still between dimensions");
	TEST_REQUIRE(v->destinationPortalIndex == -1, "dest index not consumed");
	TEST_REQUIRE(v->position == expected, "left at {0} expected {1}", v->position, expected);
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

	std::map<UString, std::list<UFOIncursion::PrimaryMission>> savedLists;
	for (auto &pref : state.ufo_mission_preference)
	{
		savedLists[pref.first] = pref.second->missionList;
		pref.second->missionList = {UFOIncursion::PrimaryMission::Overspawn};
	}
	struct RestorePrefs
	{
		GameState &state;
		std::map<UString, std::list<UFOIncursion::PrimaryMission>> saved;
		~RestorePrefs()
		{
			for (auto &pref : state.ufo_mission_preference)
			{
				auto it = saved.find(pref.first);
				if (it != saved.end())
				{
					pref.second->missionList = it->second;
				}
			}
		}
	} restorePrefs{state, std::move(savedLists)};

	auto alienCity = state.cities["CITYMAP_ALIEN"];
	TEST_REQUIRE(alienCity && alienCity->size.z > 0, "alien city has no z size");
	const Vec3<float> pos = {40.0f, 40.0f, static_cast<float>(alienCity->size.z - 1)};
	auto mothership = alienCity->placeVehicle(state, {&state, "VEHICLETYPE_ALIEN_MOTHERSHIP"},
	                                          {&state, "ORG_ALIEN"}, pos, 0.0f);
	TEST_REQUIRE(mothership != nullptr, "placeVehicle mothership failed");
	TEST_REQUIRE(mothership->city.id == "CITYMAP_ALIEN", "mothership city is {0}",
	             mothership->city.id);

	state.invasion();

	// O1–O5 all require at least one mothership; startGame may already have one,
	// so invasion() can consume an older mothership and leave this spawn in the alien city.
	sp<Vehicle> invader;
	int mothershipsInHuman = 0;
	for (auto &vp : state.vehicles)
	{
		if (!vp.second || vp.second->owner.id != "ORG_ALIEN" ||
		    vp.second->type.id != "VEHICLETYPE_ALIEN_MOTHERSHIP" ||
		    vp.second->city.id != "CITYMAP_HUMAN")
		{
			continue;
		}
		mothershipsInHuman++;
		if (!invader)
		{
			invader = vp.second;
		}
	}
	TEST_REQUIRE(invader != nullptr,
	             "no Overspawn mothership entered CITYMAP_HUMAN (placed still in {0})",
	             mothership->city.id);
	TEST_REQUIRE(mothershipsInHuman >= 1, "expected a human-city mothership");
	TEST_REQUIRE(!invader->missions.empty(), "invading mothership has no missions");
	bool sawInfiltrate = false;
	bool sawAttackBuilding = false;
	for (auto &m : invader->missions)
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
	             (int)invader->missions.front().type, (int)invader->missions.back().type);
	TEST_REQUIRE(!sawAttackBuilding, "Overspawn mothership was given AttackBuilding");
	return true;
}

// U1(a): UFO2P FUN_0003a910 @ object-page file 0x2A90F decrements vehicle +0x171
// (UFO_mission_data +0x1B) whenever a UFO reaches a mission destination, and at
// zero either retargets or clears the target. See
// docs/original-game/findings/U1-U2-V1-incursion.md U1(a) and
// VehicleMission::advanceMissionCounterOnArrival().
static bool test_ufo_mission_counter_decrements_on_arrival()
{
	auto &state = *g_state;

	StateRef<Building> target;
	for (auto &b : state.buildings)
	{
		if (b.second && b.second->isAlive())
		{
			target = {&state, b.first};
			break;
		}
	}
	TEST_REQUIRE(!!target, "no alive building available to seed the mission target");

	Vehicle v;
	v.city = {&state, "CITYMAP_HUMAN"};

	VehicleMission mission;
	mission.type = VehicleMission::MissionType::AttackBuilding;
	mission.targetBuilding = target;
	mission.missionCounter = 3;

	const bool result = mission.advanceMissionCounterOnArrival(state, v);
	TEST_REQUIRE(result, "a non-zero decrement must not cancel the mission");
	TEST_REQUIRE(mission.missionCounter == 2, "missionCounter should be 2, got {0}",
	             mission.missionCounter);
	TEST_REQUIRE(mission.targetBuilding == target, "target building must be unchanged {0}",
	             mission.targetBuilding.id);
	TEST_REQUIRE(!mission.cancelled, "mission must not be cancelled");
	return true;
}

static bool test_ufo_mission_counter_zero_picks_new_target()
{
	auto &state = *g_state;

	StateRef<Building> nearby;
	for (auto &b : state.buildings)
	{
		if (b.second && b.second->isAlive() && b.second->city.id == "CITYMAP_HUMAN")
		{
			nearby = {&state, b.first};
			break;
		}
	}
	TEST_REQUIRE(!!nearby, "no alive CITYMAP_HUMAN building available");

	Vehicle v;
	v.city = {&state, "CITYMAP_HUMAN"};
	// Stand right on top of the building so acquireTargetBuilding()'s distance
	// check (TARGET_BUILDING_DISTANCE_LIMIT) finds it.
	v.position = {(float)((nearby->bounds.p0.x + nearby->bounds.p1.x) / 2),
	             (float)((nearby->bounds.p0.y + nearby->bounds.p1.y) / 2), 0.0f};

	VehicleMission mission;
	mission.type = VehicleMission::MissionType::AttackBuilding;
	mission.targetBuilding = nearby;
	mission.missionCounter = 1;

	const bool result = mission.advanceMissionCounterOnArrival(state, v);
	TEST_REQUIRE(result, "reaching zero with an available building must not cancel");
	TEST_REQUIRE(mission.missionCounter == 0, "missionCounter should reach 0, got {0}",
	             mission.missionCounter);
	TEST_REQUIRE(!!mission.targetBuilding, "a new target building should have been acquired");
	TEST_REQUIRE(!mission.cancelled, "mission must not be cancelled when a target is found");
	return true;
}

static bool test_ufo_mission_counter_zero_without_building_clears_target()
{
	auto &state = *g_state;

	// An isolated city with no buildings guarantees acquireTargetBuilding() fails,
	// exercising the "no building found" branch without depending on real map
	// layout / distance assumptions.
	state.cities["CITYMAP_TEST_UFO_MISSION_COUNTER_EMPTY"] = mksp<City>();

	StateRef<Building> priorTarget;
	for (auto &b : state.buildings)
	{
		if (b.second && b.second->isAlive())
		{
			priorTarget = {&state, b.first};
			break;
		}
	}
	TEST_REQUIRE(!!priorTarget, "no alive building available to seed the prior target");

	Vehicle v;
	v.city = {&state, "CITYMAP_TEST_UFO_MISSION_COUNTER_EMPTY"};

	VehicleMission mission;
	mission.type = VehicleMission::MissionType::AttackBuilding;
	mission.targetBuilding = priorTarget;
	mission.missionCounter = 1;

	const bool result = mission.advanceMissionCounterOnArrival(state, v);
	TEST_REQUIRE(!result, "reaching zero with no building available must signal cancellation");
	TEST_REQUIRE(mission.missionCounter == 0, "missionCounter should reach 0, got {0}",
	             mission.missionCounter);
	TEST_REQUIRE(!mission.targetBuilding, "target building should be cleared when none is found");
	TEST_REQUIRE(mission.cancelled, "mission should be cancelled when no target is found");
	return true;
}

// U1(a) call-site lock. The three tests above drive
// VehicleMission::advanceMissionCounterOnArrival() directly, which leaves the *hook* untested --
// the call inside VehicleMission::start()'s MissionType::AttackBuilding re-plan branch. Delete
// that one line and all three still pass. This drives the real start() instead, so the counter
// can only move if the production call site is still there.
//
// Preconditions for reaching the branch (vehiclemission.cpp, AttackBuilding case): targetBuilding
// must already be set (or start() cancels before reaching it), takeOffCheck() must return false
// (true once the vehicle has a tileObject, which placeVehicle gives it), and currentPlannedPath
// must be empty (true for a fresh mission). start() then decrements and re-plans through
// setPathTo, which needs the real extracted city map -- hence placeVehicle on CITYMAP_HUMAN
// rather than the synthetic City the direct-call tests above can get away with.
static bool test_ufo_mission_counter_decrements_from_mission_start()
{
	auto &state = *g_state;
	auto cityIt = state.cities.find("CITYMAP_HUMAN");
	TEST_REQUIRE(cityIt != state.cities.end(), "no CITYMAP_HUMAN city in gamestate");
	auto city = cityIt->second;

	StateRef<Building> target;
	for (auto &b : state.buildings)
	{
		if (b.second && b.second->isAlive() && b.second->city.id == "CITYMAP_HUMAN")
		{
			target = {&state, b.first};
			break;
		}
	}
	TEST_REQUIRE(!!target, "no alive CITYMAP_HUMAN building available to seed the mission target");

	const Vec3<float> spawn{(float)((target->bounds.p0.x + target->bounds.p1.x) / 2) + 0.5f,
	                        (float)((target->bounds.p0.y + target->bounds.p1.y) / 2) + 0.5f, 4.5f};
	sp<Vehicle> v;
	for (auto &t : state.vehicle_types)
	{
		if (!t.second || t.second->type != VehicleType::Type::UFO)
		{
			continue;
		}
		v = city->placeVehicle(state, {&state, t.first}, state.getAliens(), spawn, 0.0f);
		if (v)
		{
			break;
		}
	}
	TEST_REQUIRE(v != nullptr, "could not place any UFO-type vehicle in CITYMAP_HUMAN");
	TEST_REQUIRE((bool)v->tileObject, "placed UFO has no tile object, so takeOffCheck() would fire");

	VehicleMission mission;
	mission.type = VehicleMission::MissionType::AttackBuilding;
	mission.targetBuilding = target;
	mission.missionCounter = 3;

	mission.start(state, *v);

	TEST_REQUIRE(mission.missionCounter == 2,
	             "VehicleMission::start() left missionCounter at {0}, expected 2 -- the "
	             "advanceMissionCounterOnArrival() call in the AttackBuilding re-plan branch is "
	             "not being reached",
	             mission.missionCounter);
	TEST_CHECK(!mission.cancelled, "a non-zero decrement must not cancel the mission");
	TEST_CHECK(mission.targetBuilding == target,
	           "target building must be unchanged while the counter is still above zero, got {0}",
	           mission.targetBuilding.id);
	return true;
}

static int growthCount(const UFOGrowth &growth, const UString &typeId)
{
	for (auto &entry : growth.vehicleTypeList)
	{
		if (entry.first == typeId)
		{
			return entry.second;
		}
	}
	return 0;
}

static bool test_ufo_growth_rates_match_exe()
{
	auto &state = *g_state;
	auto limitIt = state.ufo_growth_lists.find("UFO_GROWTH_LIMIT");
	TEST_REQUIRE(limitIt != state.ufo_growth_lists.end() && limitIt->second,
	             "UFO_GROWTH_LIMIT missing");
	const auto &limit = *limitIt->second;
	// UFO2P non-4 0x155010 fleet caps (hexa craft 0..9).
	TEST_REQUIRE(growthCount(limit, "VEHICLETYPE_ALIEN_PROBE") == 15, "limit probe");
	TEST_REQUIRE(growthCount(limit, "VEHICLETYPE_ALIEN_SCOUT") == 15, "limit scout");
	TEST_REQUIRE(growthCount(limit, "VEHICLETYPE_ALIEN_TRANSPORTER") == 6, "limit transporter");
	TEST_REQUIRE(growthCount(limit, "VEHICLETYPE_ALIEN_FAST_ATTACK_SHIP") == 6,
	             "limit fast attack");
	TEST_REQUIRE(growthCount(limit, "VEHICLETYPE_ALIEN_DESTROYER") == 6, "limit destroyer");
	TEST_REQUIRE(growthCount(limit, "VEHICLETYPE_ALIEN_ASSAULT_SHIP") == 6, "limit assault");
	TEST_REQUIRE(growthCount(limit, "VEHICLETYPE_ALIEN_BOMBER") == 6, "limit bomber");
	TEST_REQUIRE(growthCount(limit, "VEHICLETYPE_ALIEN_ESCORT") == 6, "limit escort");
	TEST_REQUIRE(growthCount(limit, "VEHICLETYPE_ALIEN_BATTLESHIP") == 4, "limit battleship");
	TEST_REQUIRE(growthCount(limit, "VEHICLETYPE_ALIEN_MOTHERSHIP") == 2, "limit mothership");
	TEST_REQUIRE(limit.vehicleTypeList.size() == 10, "LIMIT list size {0} (patch overlay doubles)",
	             limit.vehicleTypeList.size());

	auto week1 = UFOGrowth::selectForWeek(state, 1);
	TEST_REQUIRE(week1 && week1->week == 1, "week 1 growth missing");
	TEST_REQUIRE(week1->vehicleTypeList.size() == 2, "week1 list size {0} (patch overlay doubles)",
	             week1->vehicleTypeList.size());
	TEST_REQUIRE(growthCount(*week1, "VEHICLETYPE_ALIEN_PROBE") == 9, "week1 probe");
	TEST_REQUIRE(growthCount(*week1, "VEHICLETYPE_ALIEN_SCOUT") == 9, "week1 scout");

	auto fallback = UFOGrowth::selectForWeek(state, 99);
	TEST_REQUIRE(fallback, "DEFAULT growth missing");
	TEST_REQUIRE(growthCount(*fallback, "VEHICLETYPE_ALIEN_MOTHERSHIP") == 1, "default mothership");
	return true;
}

static bool test_manufacture_dimension_probe()
{
	auto &state = *g_state;
	auto it = state.research.topics.find("MANUFACTURE_DIMENSION_PROBE");
	TEST_REQUIRE(it != state.research.topics.end() && it->second,
	             "MANUFACTURE_DIMENSION_PROBE missing");
	const auto &topic = *it->second;
	// UFO2P non-4 manufacturing_data record 0 at 0x13FD34: hours 25000, cost 6000, craft 20.
	TEST_REQUIRE(topic.type == ResearchTopic::Type::Engineering, "type");
	TEST_REQUIRE(topic.man_hours == 25000, "hours {0}", topic.man_hours);
	TEST_REQUIRE(topic.cost == 6000, "cost {0}", topic.cost);
	TEST_REQUIRE(topic.required_lab_size == ResearchTopic::LabSize::Large, "lab size");
	TEST_REQUIRE(topic.item_type == ResearchTopic::ItemType::Craft, "item type");
	TEST_REQUIRE(topic.itemId == "VEHICLETYPE_DIMENSION_PROBE", "itemId {0}", topic.itemId);
	TEST_REQUIRE(topic.dependencies.research.size() == 1,
	             "probe research groups {0} (extract+patch doubles)",
	             topic.dependencies.research.size());
	bool sawProbeResearch = false;
	for (auto &dep : topic.dependencies.research)
	{
		for (auto &t : dep.topics)
		{
			if (t.id == "RESEARCH_DIMENSION_PROBE")
			{
				sawProbeResearch = true;
			}
		}
	}
	TEST_REQUIRE(sawProbeResearch, "prereq is not RESEARCH_DIMENSION_PROBE");
	return true;
}

static bool hasExactDep(const ResearchTopic &topic, ResearchDependency::Type type,
                        const std::vector<UString> &required)
{
	for (const auto &dep : topic.dependencies.research)
	{
		if (dep.type != type || dep.topics.size() != required.size())
		{
			continue;
		}
		bool match = true;
		for (const auto &id : required)
		{
			bool found = false;
			for (const auto &t : dep.topics)
			{
				if (t.id == id)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				match = false;
				break;
			}
		}
		if (match)
		{
			return true;
		}
	}
	return false;
}

static bool hasExactAll(const ResearchTopic &topic, const std::vector<UString> &required)
{
	return hasExactDep(topic, ResearchDependency::Type::All, required);
}

static bool test_research_prereq_all_graphs()
{
	auto &state = *g_state;
	// UFO2P non-4 research_data prereqTech[3] at 0x13EE80. unknown2==0 → All.
	// Dimension Probe (record 2) is All of the same three sciences that
	// Quantum Lab (record 45, unknown2==1) treats as Any.
	struct Case
	{
		const char *topic;
		std::vector<UString> techs;
	};
	const Case cases[] = {
	    {"RESEARCH_DIMENSION_PROBE",
	     {"RESEARCH_ALIEN_PROPULSION_SYSTEM", "RESEARCH_ALIEN_CONTROL_SYSTEM",
	      "RESEARCH_ALIEN_ENERGY_SOURCE"}},
	    {"RESEARCH_ADVANCED_WORKSHOP", {"RESEARCH_DIMENSION_PROBE"}},
	    {"RESEARCH_ADVANCED_ALIEN_CONTAINMENT", {"RESEARCH_ADVANCED_QUANTUM_PHYSICS_LAB"}},
	};
	for (const auto &c : cases)
	{
		auto it = state.research.topics.find(c.topic);
		TEST_REQUIRE(it != state.research.topics.end() && it->second, "{0} missing", c.topic);
		TEST_REQUIRE(hasExactAll(*it->second, c.techs), "{0} lost EXE All graph", c.topic);
		for (const auto &dep : it->second->dependencies.research)
		{
			TEST_REQUIRE(dep.type != ResearchDependency::Type::Any, "{0} must not be Any", c.topic);
		}
	}
	return true;
}

static bool test_research_prereq_unknown2_any()
{
	auto &state = *g_state;
	// FUN_000aa7a8 @ VA 0xAA7A8 / file 0x10CE4C: JMP [unknown2*4+0x9A794] at
	// file 0x10CF9F (table 0x10CE38). Case 1 is Any of prereqTech[3].
	// Records 36 / 43 / 45. Fails if the extractor still emits All.
	struct Case
	{
		const char *topic;
		std::vector<UString> techs;
	};
	const Case cases[] = {
	    {"RESEARCH_THE_ALIEN_GENETIC_STRUCTURE",
	     {"RESEARCH_MULTIWORM_EGG_AUTOPSY", "RESEARCH_MULTIWORM_AUTOPSY",
	      "RESEARCH_HYPERWORM_AUTOPSY"}},
	    {"RESEARCH_ADVANCED_SECURITY_STATION",
	     {"RESEARCH_LIGHT_DISRUPTOR_BEAM", "RESEARCH_MEDIUM_DISRUPTOR_BEAM",
	      "RESEARCH_DISRUPTOR_INVERSION_BOMB"}},
	    {"RESEARCH_ADVANCED_QUANTUM_PHYSICS_LAB",
	     {"RESEARCH_ALIEN_PROPULSION_SYSTEM", "RESEARCH_ALIEN_CONTROL_SYSTEM",
	      "RESEARCH_ALIEN_ENERGY_SOURCE"}},
	};
	for (const auto &c : cases)
	{
		auto it = state.research.topics.find(c.topic);
		TEST_REQUIRE(it != state.research.topics.end() && it->second, "{0} missing", c.topic);
		TEST_REQUIRE(hasExactDep(*it->second, ResearchDependency::Type::Any, c.techs),
		             "{0} lost EXE Any graph", c.topic);
		for (const auto &dep : it->second->dependencies.research)
		{
			TEST_REQUIRE(dep.type != ResearchDependency::Type::All, "{0} must not be All", c.topic);
		}
	}
	return true;
}

static bool test_research_prereq_unknown1_any()
{
	auto &state = *g_state;
	// FUN_000aa7a8 unknown1==1 @ file 0x10CED7: Any of three typed item gates.
	// Record 62 unknown3=0000 → Light or Medium or Heavy.
	// Record 63 unknown3=00FF → Medium or Heavy.
	// Record 70 unknown3=FFFF → Small Shield only (Large type 0xFF is omitted).
	auto light = state.research.topics.find("RESEARCH_LIGHT_DISRUPTOR_BEAM");
	auto medium = state.research.topics.find("RESEARCH_MEDIUM_DISRUPTOR_BEAM");
	auto small = state.research.topics.find("RESEARCH_SMALL_DISRUPTION_SHIELD");
	TEST_REQUIRE(light != state.research.topics.end() && light->second, "LIGHT missing");
	TEST_REQUIRE(medium != state.research.topics.end() && medium->second, "MEDIUM missing");
	TEST_REQUIRE(small != state.research.topics.end() && small->second, "SMALL SHIELD missing");
	TEST_REQUIRE(light->second->dependencies.items.type == ItemDependency::Type::Any,
	             "Light item combinator is not Any");
	TEST_REQUIRE(medium->second->dependencies.items.type == ItemDependency::Type::Any,
	             "Medium item combinator is not Any");
	TEST_REQUIRE(small->second->dependencies.items.type == ItemDependency::Type::Any,
	             "Small Shield item combinator is not Any");

	StateRef<VEquipmentType> lightEq{&state, "VEQUIPMENTTYPE_LIGHT_DISRUPTOR_BEAM"};
	StateRef<VEquipmentType> mediumEq{&state, "VEQUIPMENTTYPE_MEDIUM_DISRUPTOR_BEAM"};
	StateRef<VEquipmentType> heavyEq{&state, "VEQUIPMENTTYPE_HEAVY_DISRUPTOR_BEAM"};
	StateRef<VEquipmentType> smallEq{&state, "VEQUIPMENTTYPE_SMALL_DISRUPTION_SHIELD"};
	StateRef<VEquipmentType> largeEq{&state, "VEQUIPMENTTYPE_LARGE_DISRUPTION_SHIELD"};
	auto has = [](const ItemDependency &items, const StateRef<VEquipmentType> &eq) -> bool
	{
		auto it = items.vehicleItemsRequired.find(eq);
		return it != items.vehicleItemsRequired.end() && it->second == 1;
	};
	TEST_REQUIRE(has(light->second->dependencies.items, lightEq) &&
	                 has(light->second->dependencies.items, mediumEq) &&
	                 has(light->second->dependencies.items, heavyEq),
	             "Light lost EXE Any of three beams");
	TEST_REQUIRE(has(medium->second->dependencies.items, mediumEq) &&
	                 has(medium->second->dependencies.items, heavyEq) &&
	                 !has(medium->second->dependencies.items, lightEq),
	             "Medium must be Medium-or-Heavy");
	TEST_REQUIRE(has(small->second->dependencies.items, smallEq) &&
	                 !has(small->second->dependencies.items, largeEq),
	             "Small Shield must not accept Large");

	TEST_REQUIRE(!state.player_bases.empty(), "no player base");
	StateRef<Base> base{&state, state.player_bases.begin()->first};
	auto &inv = base->inventoryVehicleEquipment;
	const auto savedLight = inv[lightEq.id];
	const auto savedMedium = inv[mediumEq.id];
	const auto savedHeavy = inv[heavyEq.id];
	const auto savedSmall = inv[smallEq.id];
	const auto savedLarge = inv[largeEq.id];
	inv[lightEq.id] = 0;
	inv[mediumEq.id] = 0;
	inv[heavyEq.id] = 1;
	inv[smallEq.id] = 0;
	inv[largeEq.id] = 0;
	TEST_REQUIRE(light->second->dependencies.items.satisfied(base),
	             "Heavy must unlock Light research");
	inv[heavyEq.id] = 0;
	TEST_REQUIRE(!light->second->dependencies.items.satisfied(base),
	             "empty inventory must not unlock Light");
	inv[smallEq.id] = 1;
	TEST_REQUIRE(small->second->dependencies.items.satisfied(base),
	             "Small Shield unlocks its research");
	inv[smallEq.id] = 0;
	inv[largeEq.id] = 1;
	TEST_REQUIRE(!small->second->dependencies.items.satisfied(base),
	             "Large Shield must not unlock Small research");
	inv[lightEq.id] = savedLight;
	inv[mediumEq.id] = savedMedium;
	inv[heavyEq.id] = savedHeavy;
	inv[smallEq.id] = savedSmall;
	inv[largeEq.id] = savedLarge;
	return true;
}

static bool test_research_aa7a8_hardcoded_gates()
{
	auto &state = *g_state;
	// FUN_000aa7a8 listing @ VA 0xAAABE / file 0x10D162 (decompiler dropped
	// the MOV EBX,1 at 0xAAAEA). Record 37: All of 12..17 plus topic 36.
	// Record 38: All of 8..31. Record 44: Any of 7..33.
	std::vector<UString> cycleIds;
	cycleIds.reserve(sizeof(UFO2P_AA7A8_LIFE_CYCLE_ALL) / sizeof(UFO2P_AA7A8_LIFE_CYCLE_ALL[0]));
	for (auto *id : UFO2P_AA7A8_LIFE_CYCLE_ALL)
	{
		cycleIds.emplace_back(id);
	}
	std::vector<UString> threat;
	threat.reserve(sizeof(UFO2P_AA7A8_THREAT_ALL) / sizeof(UFO2P_AA7A8_THREAT_ALL[0]));
	for (auto *id : UFO2P_AA7A8_THREAT_ALL)
	{
		threat.emplace_back(id);
	}
	std::vector<UString> biochem;
	biochem.reserve(sizeof(UFO2P_AA7A8_BIOCHEM_ANY) / sizeof(UFO2P_AA7A8_BIOCHEM_ANY[0]));
	for (auto *id : UFO2P_AA7A8_BIOCHEM_ANY)
	{
		biochem.emplace_back(id);
	}
	TEST_REQUIRE(cycleIds.size() == 7, "life cycle is 12..17 plus topic 36");
	TEST_REQUIRE(cycleIds.back() == "RESEARCH_THE_ALIEN_GENETIC_STRUCTURE",
	             "CMP [0xDE420] is Genetic Structure");
	TEST_REQUIRE(threat.size() == 24, "threat range 8..31 is 24 topics");
	TEST_REQUIRE(biochem.size() == 27, "biochem range 7..33 is 27 topics");
	TEST_REQUIRE(biochem.front() == "RESEARCH_BRAINSUCKER_PODS", "biochem loop starts at topic 7");

	auto threatTopic = state.research.topics.find("RESEARCH_THE_REAL_ALIEN_THREAT");
	auto biochemTopic = state.research.topics.find("RESEARCH_ADVANCED_BIOCHEMISTRY_LAB");
	auto cycle = state.research.topics.find("RESEARCH_THE_ALIEN_LIFE_CYCLE");
	TEST_REQUIRE(threatTopic != state.research.topics.end() && threatTopic->second,
	             "threat missing");
	TEST_REQUIRE(biochemTopic != state.research.topics.end() && biochemTopic->second,
	             "biochem missing");
	TEST_REQUIRE(cycle != state.research.topics.end() && cycle->second, "life cycle missing");
	TEST_REQUIRE(hasExactDep(*threatTopic->second, ResearchDependency::Type::All, threat),
	             "Real Alien Threat lost EXE All of 8..31");
	TEST_REQUIRE(hasExactDep(*biochemTopic->second, ResearchDependency::Type::Any, biochem),
	             "Biochem Lab lost EXE Any of 7..33");
	TEST_REQUIRE(hasExactDep(*cycle->second, ResearchDependency::Type::All, cycleIds),
	             "Life Cycle lost EXE All of 12..17 plus Genetic Structure");

	auto pods = state.research.topics.find("RESEARCH_BRAINSUCKER_PODS");
	auto autopsy = state.research.topics.find("RESEARCH_BRAINSUCKER_AUTOPSY");
	auto genetics = state.research.topics.find("RESEARCH_THE_ALIEN_GENETIC_STRUCTURE");
	TEST_REQUIRE(pods != state.research.topics.end() && pods->second, "pods missing");
	TEST_REQUIRE(autopsy != state.research.topics.end() && autopsy->second, "autopsy missing");
	TEST_REQUIRE(genetics != state.research.topics.end() && genetics->second, "genetics missing");
	const auto savedPods = pods->second->man_hours_progress;
	const auto savedPodsStarted = pods->second->started;
	const auto savedAutopsy = autopsy->second->man_hours_progress;
	struct SavedTopic
	{
		sp<ResearchTopic> topic;
		unsigned progress = 0;
		bool started = false;
	};
	std::vector<SavedTopic> savedCycle;
	for (const auto &id : cycleIds)
	{
		auto it = state.research.topics.find(id);
		TEST_REQUIRE(it != state.research.topics.end() && it->second, "cycle topic {0} missing",
		             id);
		savedCycle.push_back({it->second, it->second->man_hours_progress, it->second->started});
		it->second->man_hours_progress = 0;
	}
	pods->second->man_hours_progress = 0;
	autopsy->second->man_hours_progress = 0;
	TEST_REQUIRE(!biochemTopic->second->dependencies.research.front().satisfied(),
	             "Biochem stays gated with no bio topic");
	pods->second->forceComplete();
	TEST_REQUIRE(biochemTopic->second->dependencies.research.front().satisfied(),
	             "Brainsucker Pods must unlock Biochem");
	TEST_REQUIRE(!threatTopic->second->dependencies.research.front().satisfied(),
	             "one complete topic must not unlock Threat");
	TEST_REQUIRE(!cycle->second->dependencies.research.front().satisfied(),
	             "Life Cycle stays gated when 12..17 and 36 are incomplete");
	for (auto &saved : savedCycle)
	{
		if (saved.topic != genetics->second)
		{
			saved.topic->forceComplete();
		}
	}
	TEST_REQUIRE(!cycle->second->dependencies.research.front().satisfied(),
	             "12..17 without Genetic Structure must not unlock Life Cycle");
	genetics->second->forceComplete();
	TEST_REQUIRE(cycle->second->dependencies.research.front().satisfied(),
	             "All of 12..17 plus Genetic Structure unlocks Life Cycle");
	for (auto &saved : savedCycle)
	{
		saved.topic->man_hours_progress = saved.progress;
		saved.topic->started = saved.started;
	}
	pods->second->man_hours_progress = savedPods;
	pods->second->started = savedPodsStarted;
	autopsy->second->man_hours_progress = savedAutopsy;
	return true;
}

static bool test_alien_building4_keeps_table_prereq()
{
	auto &state = *g_state;
	auto it = state.research.topics.find("RESEARCH_ALIEN_BUILDING_4");
	TEST_REQUIRE(it != state.research.topics.end() && it->second,
	             "RESEARCH_ALIEN_BUILDING_4 missing");
	bool sawDimension = false;
	bool sawUnlock = false;
	for (auto &dep : it->second->dependencies.research)
	{
		for (auto &t : dep.topics)
		{
			if (t.id == "RESEARCH_THE_ALIEN_DIMENSION")
			{
				sawDimension = true;
			}
			if (t.id == "RESEARCH_UNLOCK_ALIEN_BUILDING_4")
			{
				sawUnlock = true;
			}
		}
	}
	// UFO2P non-4 research_data[92] prereqTech[0] = 1 (The Alien Dimension).
	// Patch adds the visit unlock; do not op=delete that list.
	TEST_REQUIRE(sawDimension, "lost EXE prereq RESEARCH_THE_ALIEN_DIMENSION");
	TEST_REQUIRE(sawUnlock, "lost patch unlock RESEARCH_UNLOCK_ALIEN_BUILDING_4");
	return true;
}

static bool test_alien_building_exe_rows()
{
	auto &state = *g_state;
	// UFO2P non-4 research_data records 88–97 @ 0x13F820. Ten "Alien building"
	// names; each row keeps its own score / hours / prereqTech.
	struct Row
	{
		const char *topic;
		int hours;
		int score;
		bool wantsDimension;
	};
	const Row rows[] = {
	    {"RESEARCH_ALIEN_BUILDING_0", 38000, 380, false},
	    {"RESEARCH_ALIEN_BUILDING_1", 42000, 420, false},
	    {"RESEARCH_ALIEN_BUILDING_2", 32000, 320, false},
	    {"RESEARCH_ALIEN_BUILDING_3", 46000, 460, false},
	    {"RESEARCH_ALIEN_BUILDING_4", 30000, 300, true},
	    {"RESEARCH_ALIEN_BUILDING_5", 44000, 440, false},
	    {"RESEARCH_ALIEN_BUILDING_6", 34000, 340, false},
	    {"RESEARCH_ALIEN_BUILDING_7", 40000, 400, false},
	    {"RESEARCH_ALIEN_BUILDING_8", 36000, 360, false},
	    {"RESEARCH_ALIEN_BUILDING_9", 48000, 480, false},
	};
	for (const auto &row : rows)
	{
		auto it = state.research.topics.find(row.topic);
		TEST_REQUIRE(it != state.research.topics.end() && it->second, "{0} missing", row.topic);
		TEST_REQUIRE(it->second->man_hours == row.hours, "{0} hours {1}", row.topic,
		             it->second->man_hours);
		TEST_REQUIRE(it->second->score == row.score, "{0} score {1}", row.topic, it->second->score);
		bool sawDimension = false;
		for (const auto &dep : it->second->dependencies.research)
		{
			for (const auto &t : dep.topics)
			{
				if (t.id == "RESEARCH_THE_ALIEN_DIMENSION")
				{
					sawDimension = true;
				}
			}
		}
		TEST_REQUIRE(sawDimension == row.wantsDimension, "{0} Dimension gate {1}", row.topic,
		             sawDimension);
	}
	return true;
}

static bool test_manufacture_type02_ammo_ids()
{
	auto &state = *g_state;
	// UFO2P non-4 manufacturing_data type 2 indexes craft_ammo_names at 0x14B18E
	// (rec 9 → 8 Disruptor Bomb, rec 11 → 9 Stasis Bomb, rec 13 → 10 Multi-Bomb).
	// Using the manufacture name would emit DISRUPTOR_INVERSION_BOMB / STASIS_FIELD_BOMB.
	struct Case
	{
		const char *topic;
		const char *ammo;
	};
	const Case cases[] = {
	    {"MANUFACTURE_DISRUPTOR_INVERSION_BOMB", "VEQUIPMENTAMMOTYPE_DISRUPTOR_BOMB"},
	    {"MANUFACTURE_STASIS_FIELD_BOMB", "VEQUIPMENTAMMOTYPE_STASIS_BOMB"},
	    {"MANUFACTURE_DISRUPTOR_MULTI-BOMB", "VEQUIPMENTAMMOTYPE_DISRUPTOR_MULTI-BOMB"},
	};
	for (const auto &c : cases)
	{
		auto it = state.research.topics.find(c.topic);
		TEST_REQUIRE(it != state.research.topics.end() && it->second, "{0} missing", c.topic);
		TEST_REQUIRE(it->second->item_type == ResearchTopic::ItemType::VehicleEquipmentAmmo,
		             "{0} item_type", c.topic);
		TEST_REQUIRE(it->second->itemId == c.ammo, "{0} itemId {1}", c.topic, it->second->itemId);
		TEST_REQUIRE(state.vehicle_ammo.find(c.ammo) != state.vehicle_ammo.end(),
		             "{0} ammo type missing", c.ammo);
	}
	return true;
}

static bool test_manufacture_disruptor_armor_ids()
{
	auto &state = *g_state;
	// UFO2P non-4 manufacturing_data records 38–42 @ 0x13FD34 (names 0x1501F3).
	// canon_string maps '(' / ')' to '_', which used to emit
	// MANUFACTURE_DISRUPTOR_ARMOR__LEGS_ beside the patch (LEGS) key.
	// Record 37 (Dimension Destabiliser) is manufacturable=0. 42 live rows.
	struct Case
	{
		const char *topic;
		const char *item;
		unsigned hours;
		int cost;
	};
	const Case cases[] = {
	    {"MANUFACTURE_DISRUPTOR_ARMOR_(LEGS)", "AEQUIPMENTTYPE_X-COM_LEG_SHIELDS", 3400, 1200},
	    {"MANUFACTURE_DISRUPTOR_ARMOR_(TORSO)", "AEQUIPMENTTYPE_X-COM_BODY_SHIELD", 3800, 1500},
	    {"MANUFACTURE_DISRUPTOR_ARMOR_(RIGHT_ARM)", "AEQUIPMENTTYPE_X-COM_RIGHT_ARM_SHIELD", 3400,
	     1200},
	    {"MANUFACTURE_DISRUPTOR_ARMOR_(LEFT_ARM)", "AEQUIPMENTTYPE_X-COM_LEFT_ARM_SHIELD", 3400,
	     1200},
	    {"MANUFACTURE_DISRUPTOR_ARMOR_(HEAD)", "AEQUIPMENTTYPE_X-COM_HEAD_SHIELD", 3800, 1500},
	};
	const char *folded[] = {
	    "MANUFACTURE_DISRUPTOR_ARMOR__LEGS_",      "MANUFACTURE_DISRUPTOR_ARMOR__TORSO_",
	    "MANUFACTURE_DISRUPTOR_ARMOR__RIGHT_ARM_", "MANUFACTURE_DISRUPTOR_ARMOR__LEFT_ARM_",
	    "MANUFACTURE_DISRUPTOR_ARMOR__HEAD_",
	};
	int manufactureCount = 0;
	int armorNameCount = 0;
	for (const auto &pair : state.research.topics)
	{
		if (!pair.second)
		{
			continue;
		}
		if (pair.first.find("MANUFACTURE_") == 0)
		{
			manufactureCount++;
		}
		if (pair.second->name.find("Disruptor Armor (") == 0)
		{
			armorNameCount++;
		}
	}
	TEST_REQUIRE(manufactureCount == 42, "MANUFACTURE_* count {0} (extract+patch doubles armor)",
	             manufactureCount);
	TEST_REQUIRE(armorNameCount == 5, "Disruptor Armor name count {0}", armorNameCount);
	for (const auto *id : folded)
	{
		TEST_REQUIRE(state.research.topics.find(id) == state.research.topics.end(),
		             "folded extract key {0} must not remain", id);
	}
	for (const auto &c : cases)
	{
		auto it = state.research.topics.find(c.topic);
		TEST_REQUIRE(it != state.research.topics.end() && it->second, "{0} missing", c.topic);
		TEST_REQUIRE(it->second->type == ResearchTopic::Type::Engineering, "{0} type", c.topic);
		TEST_REQUIRE(it->second->item_type == ResearchTopic::ItemType::AgentEquipment,
		             "{0} item_type", c.topic);
		TEST_REQUIRE(it->second->man_hours == c.hours, "{0} hours {1}", c.topic,
		             it->second->man_hours);
		TEST_REQUIRE(it->second->cost == c.cost, "{0} cost {1}", c.topic, it->second->cost);
		TEST_REQUIRE(it->second->itemId == c.item, "{0} itemId {1}", c.topic, it->second->itemId);
		TEST_REQUIRE(state.agent_equipment.find(c.item) != state.agent_equipment.end(),
		             "{0} equipment missing", c.item);
		TEST_REQUIRE(it->second->dependencies.research.size() == 1, "{0} research groups {1}",
		             c.topic, it->second->dependencies.research.size());
		TEST_REQUIRE(
		    hasExactDep(*it->second, ResearchDependency::Type::All, {"RESEARCH_DISRUPTOR_ARMOR"}),
		    "{0} prereq is not All(RESEARCH_DISRUPTOR_ARMOR)", c.topic);
	}
	return true;
}

static bool test_craft_ammo_manufacturers()
{
	auto &state = *g_state;
	// UFO2P non-4 craft_ammo_manufacturers_data at 0x13EB6A: uint16[15] org index.
	// Zorium is org 0 (X-COM). Patch vehicle_ammo.xml must not carry
	// <manufacturer> — applyCraftAmmoManufacturers writes the EXE org.
	struct Case
	{
		const char *ammo;
		const char *org;
	};
	const Case cases[] = {
	    {"VEQUIPMENTAMMOTYPE_FUSION_POWERFUEL", "ORG_SUPERDYNAMICS"},
	    {"VEQUIPMENTAMMOTYPE_ELERIUM_115", "ORG_SOLMINE"},
	    {"VEQUIPMENTAMMOTYPE_ZORIUM", "ORG_X-COM"},
	    {"VEQUIPMENTAMMOTYPE_DISRUPTOR_BOMB", "ORG_X-COM"},
	};
	for (const auto &c : cases)
	{
		auto it = state.vehicle_ammo.find(c.ammo);
		TEST_REQUIRE(it != state.vehicle_ammo.end() && it->second, "{0} missing", c.ammo);
		TEST_REQUIRE(it->second->manufacturer.id == c.org, "{0} manufacturer {1}", c.ammo,
		             it->second->manufacturer.id);
	}
	return true;
}

static bool test_craft_ammo_economy_ids()
{
	auto &state = *g_state;
	// extractEconomy keys economy_data2 with getVAmmoId (keeps '-'). Patch
	// VAmmoType IDs fold Elerium-115 / Multi-Cannon Round. purchase() looks up
	// economy[vehicle_ammo.id].
	TEST_REQUIRE(state.economy.find("VEQUIPMENTAMMOTYPE_ELERIUM-115") == state.economy.end(),
	             "stale hyphen Elerium economy key");
	TEST_REQUIRE(state.economy.find("VEQUIPMENTAMMOTYPE_MULTI-CANNON_ROUND") == state.economy.end(),
	             "stale hyphen Multi-Cannon economy key");
	auto elerium = state.economy.find("VEQUIPMENTAMMOTYPE_ELERIUM_115");
	TEST_REQUIRE(elerium != state.economy.end(), "Elerium economy missing under live ammo id");
	TEST_REQUIRE(elerium->second.weekAvailable == 1 && elerium->second.basePrice == 20,
	             "Elerium week/price {0}/{1}", elerium->second.weekAvailable,
	             elerium->second.basePrice);
	auto multi = state.economy.find("VEQUIPMENTAMMOTYPE_MULTI_CANNON_ROUND");
	TEST_REQUIRE(multi != state.economy.end(), "Multi-Cannon economy missing under live ammo id");
	TEST_REQUIRE(multi->second.weekAvailable == 1 && multi->second.basePrice == 5,
	             "Multi-Cannon week/price {0}/{1}", multi->second.weekAvailable,
	             multi->second.basePrice);
	return true;
}

static bool test_vehicle_equipment_ammo_types()
{
	auto &state = *g_state;
	// UFO2P non-4 vehicle_equipment.ammo_type indexes craft_ammo_names at 0x14B18E.
	// 0xffff = none (lasers / disruptor beams).
	struct Case
	{
		const char *equipment;
		const char *ammo;
	};
	const Case cases[] = {
	    {"VEQUIPMENTTYPE_RENDOR_PLASMA_GUN", "VEQUIPMENTAMMOTYPE_ELERIUM_115"},
	    {"VEQUIPMENTTYPE_40MM_AUTO_CANNON", "VEQUIPMENTAMMOTYPE_MULTI_CANNON_ROUND"},
	    {"VEQUIPMENTTYPE_JANITOR_MISSILE_ARRAY", "VEQUIPMENTAMMOTYPE_JANITOR_MISSILE"},
	    {"VEQUIPMENTTYPE_AIRGUARD_ANTI-AIR_CANNON",
	     "VEQUIPMENTAMMOTYPE_AIRGUARD_52MM_CANNON_ROUND"},
	    {"VEQUIPMENTTYPE_GLM_AIR_DEFENSE", "VEQUIPMENTAMMOTYPE_AIR_DEFENSE_MISSILE"},
	    {"VEQUIPMENTTYPE_SD_STANDARD", "VEQUIPMENTAMMOTYPE_FUSION_POWERFUEL"},
	};
	for (const auto &c : cases)
	{
		auto it = state.vehicle_equipment.find(c.equipment);
		TEST_REQUIRE(it != state.vehicle_equipment.end() && it->second, "{0} missing", c.equipment);
		TEST_REQUIRE(it->second->ammo_type.id == c.ammo, "{0} ammo_type {1}", c.equipment,
		             it->second->ammo_type.id);
	}
	auto bolter = state.vehicle_equipment.find("VEQUIPMENTTYPE_BOLTER_4000_LASER_GUN");
	TEST_REQUIRE(bolter != state.vehicle_equipment.end() && bolter->second, "Bolter missing");
	TEST_REQUIRE(!bolter->second->ammo_type, "Bolter ammo_type {0}", bolter->second->ammo_type.id);
	return true;
}

static bool test_disruptor_multibomb_fragment()
{
	auto &state = *g_state;
	// UFO2P non-4 vehicle_weapons[24] @ 0x18B510 (parent data_idx 15 split_idx 24).
	// speed 22, accuracy raw 90 → 10, damage 65. Patch must not keep those scalars.
	auto parent = state.vehicle_equipment.find("VEQUIPMENTTYPE_DISRUPTOR_MULTI-BOMB_LAUNCHER");
	TEST_REQUIRE(parent != state.vehicle_equipment.end() && parent->second,
	             "Multi-Bomb launcher missing");
	TEST_REQUIRE(parent->second->splitIntoTypes.size() == 4, "parent split count {0}",
	             parent->second->splitIntoTypes.size());
	for (auto &ref : parent->second->splitIntoTypes)
	{
		TEST_REQUIRE(ref.id == "VEQUIPMENTTYPE_DISRUPTOR_MULTI-BOMB_FRAGMENT",
		             "parent split id {0}", ref.id);
	}
	auto frag = state.vehicle_equipment.find("VEQUIPMENTTYPE_DISRUPTOR_MULTI-BOMB_FRAGMENT");
	TEST_REQUIRE(frag != state.vehicle_equipment.end() && frag->second, "fragment missing");
	TEST_REQUIRE(frag->second->speed == 22, "fragment speed {0}", frag->second->speed);
	TEST_REQUIRE(frag->second->accuracy == 10, "fragment accuracy {0}", frag->second->accuracy);
	TEST_REQUIRE(frag->second->damage == 65, "fragment damage {0}", frag->second->damage);
	TEST_REQUIRE(frag->second->guided, "fragment must be guided");
	TEST_REQUIRE(frag->second->turn_rate == 28, "fragment turn {0}", frag->second->turn_rate);
	TEST_REQUIRE(frag->second->range == 400, "fragment range {0}", frag->second->range);
	TEST_REQUIRE(frag->second->ttl == 2400, "fragment ttl {0}", frag->second->ttl);
	TEST_REQUIRE(frag->second->tail_size == 20, "fragment tail {0}", frag->second->tail_size);
	return true;
}

static bool test_dimension_probe_manufacturer()
{
	auto &state = *g_state;
	// UFO2P non-4 vehicle_data[20] manufacturer uint16 at 0x189C8C + 20×126 = 5 (Marsec).
	auto it = state.vehicle_types.find("VEHICLETYPE_DIMENSION_PROBE");
	TEST_REQUIRE(it != state.vehicle_types.end() && it->second, "DIMENSION_PROBE missing");
	TEST_REQUIRE(it->second->manufacturer.id == "ORG_MARSEC", "Dimension Probe manufacturer {0}",
	             it->second->manufacturer.id);
	return true;
}

static bool test_research_item_prereq_gates()
{
	auto &state = *g_state;
	// UFO2P non-4 research_data prereqType 1 / prereq 41 → agent_equipment_names[41].
	auto gun = state.research.topics.find("RESEARCH_DISRUPTOR_GUN");
	TEST_REQUIRE(gun != state.research.topics.end() && gun->second, "DISRUPTOR_GUN missing");
	StateRef<AEquipmentType> gunItem{&state, "AEQUIPMENTTYPE_DISRUPTOR_GUN"};
	auto git = gun->second->dependencies.items.agentItemsRequired.find(gunItem);
	TEST_REQUIRE(git != gun->second->dependencies.items.agentItemsRequired.end() &&
	                 git->second == 1,
	             "Disruptor Gun item gate");

	// prereqType 0 / prereq 6 → vehicle_equipment_names[6] Light Disruptor Beam.
	auto beam = state.research.topics.find("RESEARCH_LIGHT_DISRUPTOR_BEAM");
	TEST_REQUIRE(beam != state.research.topics.end() && beam->second,
	             "LIGHT_DISRUPTOR_BEAM missing");
	StateRef<VEquipmentType> beamItem{&state, "VEQUIPMENTTYPE_LIGHT_DISRUPTOR_BEAM"};
	auto bit = beam->second->dependencies.items.vehicleItemsRequired.find(beamItem);
	TEST_REQUIRE(bit != beam->second->dependencies.items.vehicleItemsRequired.end() &&
	                 bit->second == 1,
	             "Light Disruptor Beam item gate");

	// Patch only sets hidden; EXE research_data[60] type 1 / prereq 37.
	auto dest = state.research.topics.find("RESEARCH_DIMENSION_DESTABILISER");
	TEST_REQUIRE(dest != state.research.topics.end() && dest->second, "DESTABILISER missing");
	StateRef<AEquipmentType> destItem{&state, "AEQUIPMENTTYPE_DIMENSION_DESTABILISER"};
	auto dit = dest->second->dependencies.items.agentItemsRequired.find(destItem);
	TEST_REQUIRE(dit != dest->second->dependencies.items.agentItemsRequired.end() &&
	                 dit->second == 1,
	             "Dimension Destabiliser item gate");

	// EXE research_data[74] type 0 / prereq 48 → last vequip name (NUL at table end).
	auto shift = state.research.topics.find("RESEARCH_DIMENSION_SHIFTER");
	TEST_REQUIRE(shift != state.research.topics.end() && shift->second, "SHIFTER missing");
	StateRef<VEquipmentType> shiftItem{&state, "VEQUIPMENTTYPE_DIMENSION_SHIFTER"};
	auto sit = shift->second->dependencies.items.vehicleItemsRequired.find(shiftItem);
	TEST_REQUIRE(sit != shift->second->dependencies.items.vehicleItemsRequired.end() &&
	                 sit->second == 1,
	             "Dimension Shifter item gate");

	// prereqType 3: live slot 1 / dead slot 16 / pod slot 13 / Overspawn dead 29.
	auto sucker = state.research.topics.find("RESEARCH_BRAINSUCKER");
	TEST_REQUIRE(sucker != state.research.topics.end() && sucker->second, "BRAINSUCKER missing");
	StateRef<AEquipmentType> liveSucker{&state, "AEQUIPMENTTYPE_BRAINSUCKER_ALIVE"};
	auto lit = sucker->second->dependencies.items.agentItemsRequired.find(liveSucker);
	TEST_REQUIRE(lit != sucker->second->dependencies.items.agentItemsRequired.end() &&
	                 lit->second == 1,
	             "Brainsucker live gate");
	auto autopsy = state.research.topics.find("RESEARCH_BRAINSUCKER_AUTOPSY");
	TEST_REQUIRE(autopsy != state.research.topics.end() && autopsy->second, "AUTOPSY missing");
	StateRef<AEquipmentType> deadSucker{&state, "AEQUIPMENTTYPE_BRAINSUCKER_DEAD"};
	auto ait = autopsy->second->dependencies.items.agentItemsRequired.find(deadSucker);
	TEST_REQUIRE(ait != autopsy->second->dependencies.items.agentItemsRequired.end() &&
	                 ait->second == 1,
	             "Brainsucker autopsy gate");
	auto pods = state.research.topics.find("RESEARCH_BRAINSUCKER_PODS");
	TEST_REQUIRE(pods != state.research.topics.end() && pods->second, "PODS missing");
	StateRef<AEquipmentType> podItem{&state, "AEQUIPMENTTYPE_BRAINSUCKER_POD"};
	auto pit = pods->second->dependencies.items.agentItemsRequired.find(podItem);
	TEST_REQUIRE(pit != pods->second->dependencies.items.agentItemsRequired.end() &&
	                 pit->second == 1,
	             "Brainsucker Pod gate");
	auto over = state.research.topics.find("RESEARCH_OVERSPAWN_AUTOPSY");
	TEST_REQUIRE(over != state.research.topics.end() && over->second, "OVERSPAWN_AUTOPSY missing");
	StateRef<AEquipmentType> deadOver{&state, "AEQUIPMENTTYPE_OVERSPAWN_DEAD"};
	auto oit = over->second->dependencies.items.agentItemsRequired.find(deadOver);
	TEST_REQUIRE(oit != over->second->dependencies.items.agentItemsRequired.end() &&
	                 oit->second == 1,
	             "Overspawn dead gate");
	TEST_REQUIRE(over->second->man_hours == 20000, "Overspawn Autopsy hours {0}",
	             over->second->man_hours);
	TEST_REQUIRE(over->second->score == 450, "Overspawn Autopsy score {0}", over->second->score);
	auto overLive = state.research.topics.find("RESEARCH_OVERSPAWN_AUTOPSY_1");
	TEST_REQUIRE(overLive != state.research.topics.end() && overLive->second,
	             "OVERSPAWN_AUTOPSY_1 missing");
	StateRef<AEquipmentType> liveOver{&state, "AEQUIPMENTTYPE_OVERSPAWN_ALIVE"};
	auto litOver = overLive->second->dependencies.items.agentItemsRequired.find(liveOver);
	TEST_REQUIRE(litOver != overLive->second->dependencies.items.agentItemsRequired.end() &&
	                 litOver->second == 1,
	             "Overspawn live gate");
	TEST_REQUIRE(overLive->second->man_hours == 25000, "Overspawn Autopsy 1 hours {0}",
	             overLive->second->man_hours);
	TEST_REQUIRE(overLive->second->score == 450, "Overspawn Autopsy 1 score {0}",
	             overLive->second->score);
	return true;
}

static bool test_ufopaedia_alien_craft_group()
{
	auto &state = *g_state;
	TEST_REQUIRE(state.ufopaedia.find("PAEDIACATEGORY_ALIEN_CRAFT") != state.ufopaedia.end(),
	             "PAEDIACATEGORY_ALIEN_CRAFT missing");
	return true;
}

static bool test_detection_weights_from_exe()
{
	auto &state = *g_state;
	auto exe = fw().data->fs.open("xcom3/ufoexe/ufo2p.exe");
	TEST_REQUIRE(static_cast<bool>(exe), "open UFO2P.EXE");
	const auto blob = exe.readAll();
	boost::crc_32_type crc;
	crc.process_bytes(blob.get(), exe.size());
	int32_t slide = 0;
	TEST_REQUIRE(ufo2pFileSlide(crc.checksum(), slide), "known UFO2P CRC");
	const auto at = [slide](std::streamoff off) { return ufo2pTableOffset(slide, off); };
	auto dwordAt = [&](std::streamoff off) -> uint32_t
	{
		exe.clear();
		exe.seekg(off, std::ios::beg);
		uint32_t v = 0;
		exe.readule32(v);
		return v;
	};
	TEST_REQUIRE(dwordAt(at(BUILDING_DETECTION_WEIGHT_OFFSET_START) + 1 * 4) == 155,
	             "EXE Senate weight");
	TEST_REQUIRE(dwordAt(at(BUILDING_DETECTION_WEIGHT_OFFSET_START) + 2 * 4) == 135,
	             "EXE Police weight");
	TEST_REQUIRE(dwordAt(at(ALIEN_DETECTION_WEIGHT_OFFSET_START)) == 1, "EXE egg det");
	TEST_REQUIRE(dwordAt(at(ALIEN_DETECTION_WEIGHT_OFFSET_START) + 11 * 4) == 20, "EXE queen det");
	TEST_REQUIRE(dwordAt(at(ALIEN_MOVEMENT_PERCENT_OFFSET_START)) == 40, "EXE egg move");
	TEST_REQUIRE(dwordAt(at(ALIEN_MOVEMENT_PERCENT_OFFSET_START) + 12 * 4) == 30,
	             "EXE micronoid move");
	// building_functions[1/2/36/37/44] at file 0x155354.
	auto senate = state.building_functions.find("BUILDINGFUNCTION_SENATE");
	TEST_REQUIRE(senate != state.building_functions.end() && senate->second, "SENATE missing");
	TEST_REQUIRE(senate->second->detectionWeight == 155, "Senate weight");
	auto police = state.building_functions.find("BUILDINGFUNCTION_POLICE_STATION");
	TEST_REQUIRE(police != state.building_functions.end() && police->second, "POLICE missing");
	TEST_REQUIRE(police->second->detectionWeight == 135, "Police weight");
	auto temple = state.building_functions.find("BUILDINGFUNCTION_TEMPLE_OF_SIRIUS");
	TEST_REQUIRE(temple != state.building_functions.end() && temple->second, "TEMPLE missing");
	TEST_REQUIRE(temple->second->detectionWeight == 0, "Temple weight");
	auto xcom = state.building_functions.find("BUILDINGFUNCTION_X-COM_BASE");
	TEST_REQUIRE(xcom != state.building_functions.end() && xcom->second, "X-COM BASE missing");
	TEST_REQUIRE(xcom->second->detectionWeight == 0, "X-COM Base weight");
	auto organ = state.building_functions.find("BUILDINGFUNCTION_ORGANIC_FACTORY");
	TEST_REQUIRE(organ != state.building_functions.end() && organ->second, "ORGANIC missing");
	TEST_REQUIRE(organ->second->detectionWeight == 100, "Organic Factory weight");

	// alien infiltrationID 0/9/11/12 at file 0x142374 / 0x1423B0.
	auto egg = state.agent_types.find("AGENTTYPE_MULTIWORM_EGG");
	TEST_REQUIRE(egg != state.agent_types.end() && egg->second, "EGG missing");
	TEST_REQUIRE(egg->second->detectionWeight == 1 && egg->second->movementPercent == 40,
	             "egg det/move");
	auto mega = state.agent_types.find("AGENTTYPE_MEGASPAWN");
	TEST_REQUIRE(mega != state.agent_types.end() && mega->second, "MEGASPAWN missing");
	TEST_REQUIRE(mega->second->detectionWeight == 10 && mega->second->movementPercent == 0,
	             "megaspawn det/move");
	auto queen = state.agent_types.find("AGENTTYPE_QUEENSPAWN");
	TEST_REQUIRE(queen != state.agent_types.end() && queen->second, "QUEEN missing");
	TEST_REQUIRE(queen->second->detectionWeight == 20 && queen->second->movementPercent == 0,
	             "queen det/move");
	auto micro = state.agent_types.find("AGENTTYPE_MICRONOID_AGGREGATE");
	TEST_REQUIRE(micro != state.agent_types.end() && micro->second, "MICRONOID missing");
	TEST_REQUIRE(micro->second->detectionWeight == 1 && micro->second->movementPercent == 30,
	             "micronoid det/move");
	auto chrysalis = state.agent_types.find("AGENTTYPE_CHRYSALIS");
	TEST_REQUIRE(chrysalis != state.agent_types.end() && chrysalis->second, "CHRYSALIS missing");
	TEST_REQUIRE(chrysalis->second->detectionWeight == 1 && chrysalis->second->movementPercent == 0,
	             "chrysalis det/move");
	auto spitter = state.agent_types.find("AGENTTYPE_SPITTER");
	TEST_REQUIRE(spitter != state.agent_types.end() && spitter->second, "SPITTER missing");
	TEST_REQUIRE(spitter->second->detectionWeight == 3 && spitter->second->movementPercent == 33,
	             "spitter det/move");
	auto popper = state.agent_types.find("AGENTTYPE_POPPER");
	TEST_REQUIRE(popper != state.agent_types.end() && popper->second, "POPPER missing");
	TEST_REQUIRE(popper->second->detectionWeight == 2 && popper->second->movementPercent == 33,
	             "popper det/move");
	auto psimorph = state.agent_types.find("AGENTTYPE_PSIMORPH");
	TEST_REQUIRE(psimorph != state.agent_types.end() && psimorph->second, "PSIMORPH missing");
	TEST_REQUIRE(psimorph->second->detectionWeight == 8 && psimorph->second->movementPercent == 0,
	             "psimorph det/move");
	return true;
}

static bool test_organic_factory_gates_ufo_growth()
{
	auto &state = *g_state;
	Building *factory = nullptr;
	for (auto &b : state.buildings)
	{
		if (b.second && b.second->function.id == "BUILDINGFUNCTION_ORGANIC_FACTORY")
		{
			factory = b.second.get();
			break;
		}
	}
	TEST_REQUIRE(factory != nullptr, "BUILDINGFUNCTION_ORGANIC_FACTORY missing");
	TEST_REQUIRE(factory->isAlive(), "organic factory should start alive");
	TEST_REQUIRE(UFOGrowth::craftFactoryIntact(state), "intact while factory lives");

	auto savedParts = factory->buildingParts;
	factory->buildingParts.clear();
	TEST_REQUIRE(!factory->isAlive(), "empty parts should kill the factory");
	TEST_REQUIRE(!UFOGrowth::craftFactoryIntact(state), "destroyed factory must stop growth");

	int before = 0;
	for (auto &vp : state.vehicles)
	{
		if (vp.second && vp.second->city.id == "CITYMAP_ALIEN")
		{
			before++;
		}
	}
	state.updateUfoGrowth();
	int after = 0;
	for (auto &vp : state.vehicles)
	{
		if (vp.second && vp.second->city.id == "CITYMAP_ALIEN")
		{
			after++;
		}
	}
	TEST_REQUIRE(after == before, "updateUfoGrowth spawned {0} after factory death (was {1})",
	             after, before);

	factory->buildingParts = savedParts;
	TEST_REQUIRE(UFOGrowth::craftFactoryIntact(state), "restored factory should be intact");
	return true;
}

static bool test_ufo_incursion_table()
{
	auto &state = *g_state;
	auto i1 = state.ufo_incursions.find("UFO_INCURSION_I1");
	TEST_REQUIRE(i1 != state.ufo_incursions.end() && i1->second, "UFO_INCURSION_I1 missing");
	TEST_REQUIRE(i1->second->primaryMission == UFOIncursion::PrimaryMission::Infiltration,
	             "I1 primary");
	TEST_REQUIRE(i1->second->priority == 1, "I1 priority");
	TEST_REQUIRE(i1->second->primaryList.size() == 2, "I1 primaryList size");
	TEST_REQUIRE(i1->second->primaryList[0].first == "VEHICLETYPE_ALIEN_MOTHERSHIP" &&
	                 i1->second->primaryList[0].second == 1,
	             "I1 mothership");
	TEST_REQUIRE(i1->second->primaryList[1].first == "VEHICLETYPE_ALIEN_BATTLESHIP" &&
	                 i1->second->primaryList[1].second == 1,
	             "I1 battleship");
	TEST_REQUIRE(i1->second->escortList.size() == 1 &&
	                 i1->second->escortList[0].first == "VEHICLETYPE_ALIEN_ESCORT" &&
	                 i1->second->escortList[0].second == 2,
	             "I1 escort×2");
	// UFO2P non-4 0x13DDFC record 0 tail; FUN_0006da88 @ file 0xD012C.
	TEST_REQUIRE(i1->second->primarySlots.size() == 2 && i1->second->escortSlots.size() == 1,
	             "I1 slot vectors");
	TEST_REQUIRE(i1->second->primarySlots[0].followVehicleType.empty() &&
	                 i1->second->primarySlots[0].zoneMode == 2 &&
	                 i1->second->primarySlots[0].missionCounter == 1 &&
	                 i1->second->primarySlots[0].scatter == 15 &&
	                 i1->second->primarySlots[0].typePercent == 20,
	             "I1 mothership tail");
	TEST_REQUIRE(i1->second->primarySlots[1].followVehicleType.empty() &&
	                 i1->second->primarySlots[1].zoneMode == 2 &&
	                 i1->second->primarySlots[1].missionCounter == 1 &&
	                 i1->second->primarySlots[1].scatter == 15 &&
	                 i1->second->primarySlots[1].typePercent == 30,
	             "I1 battleship tail");
	TEST_REQUIRE(i1->second->escortSlots[0].followVehicleType == "VEHICLETYPE_ALIEN_MOTHERSHIP" &&
	                 i1->second->escortSlots[0].zoneMode == 2 &&
	                 i1->second->escortSlots[0].missionCounter == 5 &&
	                 i1->second->escortSlots[0].scatter == 15 &&
	                 i1->second->escortSlots[0].typePercent == 20,
	             "I1 escort follows mothership (follow_slot 0)");

	// Record A1 proves +0x1B is not an index into the ten dimension-gate slots:
	// FUN_0003a910 decrements vehicle +0x171 as a mission-arrival counter.
	auto a1 = state.ufo_incursions.find("UFO_INCURSION_A1");
	TEST_REQUIRE(a1 != state.ufo_incursions.end() && a1->second &&
	                 a1->second->primarySlots.size() == 3,
	             "UFO_INCURSION_A1 slots");
	TEST_REQUIRE(a1->second->primarySlots[0].missionCounter == 12 &&
	                 a1->second->primarySlots[1].missionCounter == 12 &&
	                 a1->second->primarySlots[2].missionCounter == 12,
	             "A1 mission counters");

	auto i5 = state.ufo_incursions.find("UFO_INCURSION_I5");
	TEST_REQUIRE(
	    i5 != state.ufo_incursions.end() && i5->second && i5->second->escortSlots.size() == 1 &&
	        i5->second->escortSlots[0].followVehicleType == "VEHICLETYPE_ALIEN_ASSAULT_SHIP",
	    "I5 escort follows craft[1] assault ship");

	auto o3 = state.ufo_incursions.find("UFO_INCURSION_O3");
	TEST_REQUIRE(o3 != state.ufo_incursions.end() && o3->second &&
	                 o3->second->escortSlots.size() == 1 &&
	                 o3->second->escortSlots[0].followVehicleType.empty(),
	             "O3 role 11 has follow_slot 0xFFFF");

	auto o5 = state.ufo_incursions.find("UFO_INCURSION_O5");
	TEST_REQUIRE(o5 != state.ufo_incursions.end() && o5->second, "UFO_INCURSION_O5 missing");
	TEST_REQUIRE(o5->second->primaryMission == UFOIncursion::PrimaryMission::Overspawn,
	             "O5 primary");
	TEST_REQUIRE(o5->second->primaryList.size() == 1 &&
	                 o5->second->primaryList[0].first == "VEHICLETYPE_ALIEN_MOTHERSHIP" &&
	                 o5->second->primaryList[0].second == 1,
	             "O5 single mothership");
	TEST_REQUIRE(o5->second->escortList.empty() && o5->second->attackList.empty(),
	             "O5 extra lists");
	TEST_REQUIRE(o5->second->escortSlots.empty() && o5->second->attackSlots.empty(),
	             "O5 extra slots");

	// FUN_0006da88: constitution (VehicleType::health) × type_percent / 100; scatter clamp.
	TEST_REQUIRE(VehicleMission::clampIncursionScatter(15, 20) == 15,
	             "I1 percent keeps scatter 15");
	TEST_REQUIRE(VehicleMission::clampIncursionScatter(15, 50) == 15, "percent 50 is not > 0x32");
	TEST_REQUIRE(VehicleMission::clampIncursionScatter(15, 51) == 10,
	             "percent 51 forces scatter 10");
	TEST_REQUIRE(VehicleMission::clampIncursionScatter(20, 51) == 20,
	             "clamp only when scatter is 15");
	auto mothership = state.vehicle_types.find("VEHICLETYPE_ALIEN_MOTHERSHIP");
	auto battleship = state.vehicle_types.find("VEHICLETYPE_ALIEN_BATTLESHIP");
	auto escort = state.vehicle_types.find("VEHICLETYPE_ALIEN_ESCORT");
	TEST_REQUIRE(mothership != state.vehicle_types.end() && mothership->second, "mothership type");
	TEST_REQUIRE(battleship != state.vehicle_types.end() && battleship->second, "battleship type");
	TEST_REQUIRE(escort != state.vehicle_types.end() && escort->second, "escort type");
	TEST_REQUIRE(mothership->second->health == 2800, "mothership constitution {0}",
	             mothership->second->health);
	TEST_REQUIRE(battleship->second->health == 1800, "battleship constitution {0}",
	             battleship->second->health);
	TEST_REQUIRE(escort->second->health == 500, "escort constitution {0}", escort->second->health);
	TEST_REQUIRE(VehicleMission::incursionTypeThreshold(mothership->second->health, 20) == 560,
	             "I1 mothership +0x168");
	TEST_REQUIRE(VehicleMission::incursionTypeThreshold(battleship->second->health, 30) == 540,
	             "I1 battleship +0x168");
	TEST_REQUIRE(VehicleMission::incursionTypeThreshold(escort->second->health, 20) == 100,
	             "I1 escort +0x168");
	return true;
}

static bool test_ufo_incursion_spawn_xy()
{
	auto &state = *g_state;
	// FUN_0003b724 @ VA 0x3B724 / file 0x2B723: FUN_0005d1d8(scatter*2) then −scatter.
	state.rng.seed(0x4749ffc1);
	const auto first = VehicleMission::computeIncursionSpawnXY(state, 50, 50, 2, 15);
	state.rng.seed(0x4749ffc1);
	const auto second = VehicleMission::computeIncursionSpawnXY(state, 50, 50, 2, 15);
	TEST_REQUIRE(first.x == second.x && first.y == second.y, "spawn xy deterministic");
	TEST_REQUIRE(first.x >= 0 && first.x <= 100 && first.y >= 0 && first.y <= 100,
	             "spawn xy clamp keeps 100");

	state.rng.seed(1);
	const auto zone2Center = VehicleMission::computeIncursionSpawnXY(state, 50, 50, 2, 0);
	TEST_REQUIRE(zone2Center.x == 50 && zone2Center.y == 50,
	             "zone 2 scatter 0 from 50,50 must accept 50,50 (was 0..99 RNG)");

	state.rng.seed(1);
	const auto zone0Center = VehicleMission::computeIncursionSpawnXY(state, 50, 50, 0, 0);
	TEST_REQUIRE(!(zone0Center.x == 50 && zone0Center.y == 50),
	             "zone 0 scatter 0 from 50,50 must reject the inner 11..89 square");
	TEST_REQUIRE(
	    !(zone0Center.x > 10 && zone0Center.x < 90 && zone0Center.y > 10 && zone0Center.y < 90),
	    "zone 0 fallback must stay off the inner square");

	state.rng.seed(1);
	const auto zone2Edge = VehicleMission::computeIncursionSpawnXY(state, 10, 10, 2, 0);
	TEST_REQUIRE(zone2Edge.x >= 25 && zone2Edge.x <= 75 && zone2Edge.y >= 25 && zone2Edge.y <= 75,
	             "zone 2 fallback is rand16(50)+25");

	auto i1 = state.ufo_incursions.find("UFO_INCURSION_I1");
	TEST_REQUIRE(i1 != state.ufo_incursions.end() && i1->second, "I1 for spawn tail");
	TEST_REQUIRE(i1->second->primarySlots[0].zoneMode == 2 &&
	                 i1->second->primarySlots[0].scatter == 15,
	             "I1 uses zone 2 scatter 15");
	return true;
}

static bool test_ufo_incursion_follow_type()
{
	auto &state = *g_state;
	TEST_REQUIRE(state.current_city.id == "CITYMAP_HUMAN", "current_city is {0}",
	             state.current_city.id);
	auto alienCity = state.cities["CITYMAP_ALIEN"];
	TEST_REQUIRE(alienCity && alienCity->size.z > 0, "alien city has no z size");

	std::map<UString, std::list<UFOIncursion::PrimaryMission>> savedLists;
	for (auto &pref : state.ufo_mission_preference)
	{
		savedLists[pref.first] = pref.second->missionList;
		pref.second->missionList = {UFOIncursion::PrimaryMission::Infiltration};
	}
	struct RestorePrefs
	{
		GameState &state;
		std::map<UString, std::list<UFOIncursion::PrimaryMission>> saved;
		~RestorePrefs()
		{
			for (auto &pref : state.ufo_mission_preference)
			{
				auto it = saved.find(pref.first);
				if (it != saved.end())
				{
					pref.second->missionList = it->second;
				}
			}
		}
	} restorePrefs{state, std::move(savedLists)};

	const Vec3<float> pos = {40.0f, 40.0f, static_cast<float>(alienCity->size.z - 1)};
	StateRef<Organisation> aliens{&state, "ORG_ALIEN"};
	TEST_REQUIRE(!!aliens, "ORG_ALIEN missing");
	auto mothership =
	    alienCity->placeVehicle(state, {&state, "VEHICLETYPE_ALIEN_MOTHERSHIP"}, aliens, pos, 0.0f);
	auto battleship =
	    alienCity->placeVehicle(state, {&state, "VEHICLETYPE_ALIEN_BATTLESHIP"}, aliens, pos, 0.0f);
	auto escortA =
	    alienCity->placeVehicle(state, {&state, "VEHICLETYPE_ALIEN_ESCORT"}, aliens, pos, 0.0f);
	auto escortB =
	    alienCity->placeVehicle(state, {&state, "VEHICLETYPE_ALIEN_ESCORT"}, aliens, pos, 0.0f);
	TEST_REQUIRE(mothership && battleship && escortA && escortB, "failed to place I1 fleet");

	state.invasion();

	int followMothership = 0;
	int followBattleship = 0;
	for (auto &vp : state.vehicles)
	{
		if (!vp.second || vp.second->owner.id != "ORG_ALIEN" ||
		    vp.second->type.id != "VEHICLETYPE_ALIEN_ESCORT" ||
		    vp.second->city.id != "CITYMAP_HUMAN")
		{
			continue;
		}
		for (auto &m : vp.second->missions)
		{
			if (m.type != VehicleMission::MissionType::FollowVehicle || !m.targetVehicle)
			{
				continue;
			}
			if (m.targetVehicle->type.id == "VEHICLETYPE_ALIEN_MOTHERSHIP")
			{
				followMothership++;
			}
			if (m.targetVehicle->type.id == "VEHICLETYPE_ALIEN_BATTLESHIP")
			{
				followBattleship++;
			}
		}
	}
	TEST_REQUIRE(followMothership >= 1, "I1 escorts must follow mothership (follow_slot 0)");
	TEST_REQUIRE(followBattleship == 0, "I1 escorts must not follow battleship");
	return true;
}

static bool test_subversion_prefers_one_known_base()
{
	auto &state = *g_state;
	auto alienCity = state.cities["CITYMAP_ALIEN"];
	TEST_REQUIRE(alienCity && alienCity->size.z > 0, "alien city missing");

	StateRef<Base> knownBase;
	for (auto &entry : state.player_bases)
	{
		if (entry.second && entry.second->building && entry.second->building->isAlive())
		{
			knownBase = {&state, entry.first};
			break;
		}
	}
	TEST_REQUIRE(knownBase && knownBase->building, "live X-COM base missing");

	std::map<UString, std::list<UFOIncursion::PrimaryMission>> savedLists;
	for (auto &pref : state.ufo_mission_preference)
	{
		savedLists[pref.first] = pref.second->missionList;
		pref.second->missionList = {UFOIncursion::PrimaryMission::Subversion};
	}
	const bool savedKnown = knownBase->knownToAliens;
	knownBase->knownToAliens = true;

	const UString customTypeId = "VEHICLETYPE_TEST_KNOWN_BASE_TARGET";
	auto customType = mksp<VehicleType>();
	customType->name = "Known Base Target Test";
	customType->type = VehicleType::Type::UFO;
	customType->health = 100;
	state.vehicle_types[customTypeId] = customType;

	const UString incursionId = "UFO_INCURSION_TEST_KNOWN_BASE_TARGET";
	auto incursion = mksp<UFOIncursion>();
	incursion->primaryMission = UFOIncursion::PrimaryMission::Subversion;
	incursion->priority = 0;
	incursion->primaryList.emplace_back(customTypeId, 2);
	incursion->primarySlots.emplace_back();
	state.ufo_incursions[incursionId] = incursion;

	StateRef<Organisation> aliens{&state, "ORG_ALIEN"};
	auto createInvader = [&]()
	{
		auto vehicle = mksp<Vehicle>();
		vehicle->type = {&state, customTypeId};
		vehicle->owner = aliens;
		vehicle->city = {&state, "CITYMAP_ALIEN"};
		vehicle->health = customType->health;
		const UString id = Vehicle::generateObjectID(state);
		state.vehicles[id] = vehicle;
		return std::make_pair(id, vehicle);
	};
	auto first = createInvader();
	auto second = createInvader();

	struct Restore
	{
		GameState &state;
		StateRef<Base> base;
		bool known;
		std::map<UString, std::list<UFOIncursion::PrimaryMission>> preferences;
		UString incursionId;
		UString vehicleTypeId;
		std::vector<std::pair<UString, sp<Vehicle>>> vehicles;
		~Restore()
		{
			for (auto &pref : state.ufo_mission_preference)
			{
				auto it = preferences.find(pref.first);
				if (it != preferences.end())
				{
					pref.second->missionList = it->second;
				}
			}
			if (base)
			{
				base->knownToAliens = known;
			}
			state.ufo_incursions.erase(incursionId);
			for (auto &entry : vehicles)
			{
				auto &vehicle = entry.second;
				if (vehicle->tileObject)
				{
					vehicle->removeFromMap(state);
				}
				state.vehicles.erase(entry.first);
			}
			state.vehicle_types.erase(vehicleTypeId);
		}
	} restore{state,       knownBase,    savedKnown,     std::move(savedLists),
	          incursionId, customTypeId, {first, second}};

	state.invasion();

	int subversionMissions = 0;
	int targetedKnownBase = 0;
	int fallbackTargets = 0;
	for (auto &entry : restore.vehicles)
	{
		auto &vehicle = entry.second;
		for (const auto &mission : vehicle->missions)
		{
			if (mission.type != VehicleMission::MissionType::InfiltrateSubvert || !mission.subvert)
			{
				continue;
			}
			subversionMissions++;
			if (mission.targetBuilding == knownBase->building)
			{
				targetedKnownBase++;
			}
			else if (!mission.targetBuilding)
			{
				fallbackTargets++;
			}
		}
	}
	TEST_REQUIRE(subversionMissions == 2, "subversion mission count {0}", subversionMissions);
	TEST_REQUIRE(targetedKnownBase == 1, "known-base target count {0}", targetedKnownBase);
	TEST_REQUIRE(fallbackTargets == 1, "fallback target count {0}", fallbackTargets);
	return true;
}

static bool test_militarized_from_org_type()
{
	TEST_REQUIRE(!Organisation::militarizedFromType(0), "type 0");
	TEST_REQUIRE(Organisation::militarizedFromType(1), "type 1 Megapol");
	TEST_REQUIRE(!Organisation::militarizedFromType(2), "type 2");
	TEST_REQUIRE(Organisation::militarizedFromType(3), "type 3 gangs");
	TEST_REQUIRE(!Organisation::militarizedFromType(8), "type 8 X-COM");
	return true;
}

static bool test_nearby_intact_buildings()
{
	// 15-tile / 15-candidate rule used by alienMovement and post-battle retreat.
	const std::vector<Rect<int>> bounds = {
	    {{0, 0}, {2, 2}},   // center 1,1
	    {{10, 0}, {12, 2}}, // center 11,1  dist 10
	    {{20, 0}, {22, 2}}, // center 21,1  dist 20 — out of range
	    {{4, 0}, {6, 2}},   // center 5,1   dist 4
	    {{8, 0}, {10, 2}},  // center 9,1   dist 8
	};
	const std::vector<bool> intact = {true, true, true, false, true};
	const auto origin = Vec2<int>{1, 1};
	const auto ranked = Building::rankNearbyIntact(bounds, intact, origin);
	TEST_REQUIRE(ranked.size() == 2, "ranked size {0}", ranked.size());
	TEST_REQUIRE(ranked[0] == 4, "closest intact should be index 4, got {0}", ranked[0]);
	TEST_REQUIRE(ranked[1] == 1, "second should be index 1, got {0}", ranked[1]);

	const auto none = Building::rankNearbyIntact(bounds, intact, origin, 3, 15);
	TEST_REQUIRE(none.empty(), "range 3 should exclude all");
	return true;
}

static bool test_unmanned_ufo_loot()
{
	auto &state = *g_state;
	Vehicle *recoverer = nullptr;
	for (auto &v : state.vehicles)
	{
		if (v.second && v.second->owner == state.getPlayer() && v.second->homeBuilding)
		{
			recoverer = v.second.get();
			break;
		}
	}
	TEST_REQUIRE(recoverer != nullptr, "no player vehicle with a home building");

	StateRef<VEquipmentType> eq;
	for (auto &e : state.vehicle_equipment)
	{
		if (e.second && e.second->store_space > 0)
		{
			eq = {&state, e.first};
			break;
		}
	}
	TEST_REQUIRE(!!eq, "no vehicle equipment with store_space");

	Vehicle recovered;
	recovered.loot.push_back(eq);
	recovered.loot.push_back(eq);
	const auto cargoBefore = recoverer->cargo.size();
	recoverer->loadUnmannedUfoLoot(state, recovered);
	TEST_REQUIRE(recovered.loot.empty(), "recovered loot not cleared");
	TEST_REQUIRE(recoverer->cargo.size() == cargoBefore + 1, "cargo entries {0}",
	             recoverer->cargo.size());
	const auto &c = recoverer->cargo.back();
	TEST_REQUIRE(c.id == eq.id, "cargo id {0}", c.id);
	TEST_REQUIRE(c.count == 2, "cargo count {0}", c.count);
	TEST_REQUIRE(c.cost == 0, "salvage must be free");
	TEST_REQUIRE(c.destination == recoverer->homeBuilding, "destination {0}", c.destination.id);
	recoverer->cargo.pop_back();
	return true;
}

static std::vector<UString> lootIds(const Organisation &org, Organisation::LootPriority priority)
{
	std::vector<UString> ids;
	const auto it = org.loot.find(priority);
	if (it == org.loot.end())
	{
		return ids;
	}
	for (const auto &item : it->second)
	{
		ids.push_back(item.id);
	}
	return ids;
}

static bool expectLoot(const Organisation &org, Organisation::LootPriority priority,
                       const std::vector<UString> &expected, const char *label)
{
	const auto ids = lootIds(org, priority);
	TEST_REQUIRE(ids.size() >= expected.size(), "{0} loot size {1} < {2}", label, ids.size(),
	             expected.size());
	for (size_t i = 0; i < expected.size(); i++)
	{
		TEST_REQUIRE(ids[i] == expected[i], "{0}[{1}] is {2} expected {3}", label, i, ids[i],
		             expected[i]);
	}
	return true;
}

static bool test_org_raid_loot_table()
{
	auto &state = *g_state;
	// UFO2P non-4 organisation_raid_loot_data at 0x192184 (28×60, uint32[3][5]).
	// Empty slot is index 0; 85 is Elerium. Civilian is filled with nullptrs.
	auto megapol = state.organisations.find("ORG_MEGAPOL");
	TEST_REQUIRE(megapol != state.organisations.end() && megapol->second, "ORG_MEGAPOL missing");
	if (!expectLoot(*megapol->second, Organisation::LootPriority::A,
	                {"AEQUIPMENTTYPE_MEGAPOL_AP_GRENADE", "AEQUIPMENTTYPE_MEGAPOL_STUN_GRENADE",
	                 "AEQUIPMENTTYPE_MEGAPOL_PLASMA_GUN", "AEQUIPMENTTYPE_MEGAPOL_LASER_SNIPER_GUN",
	                 "AEQUIPMENTTYPE_ELERIUM"},
	                "Megapol A"))
	{
		return false;
	}
	if (!expectLoot(*megapol->second, Organisation::LootPriority::B,
	                {"AEQUIPMENTTYPE_MEGAPOL_STUN_GRAPPLE", "AEQUIPMENTTYPE_MEDI-KIT",
	                 "AEQUIPMENTTYPE_MEGAPOL_AUTO_CANNON", "AEQUIPMENTTYPE_ELERIUM",
	                 "AEQUIPMENTTYPE_ELERIUM"},
	                "Megapol B"))
	{
		return false;
	}
	if (!expectLoot(*megapol->second, Organisation::LootPriority::C,
	                {"AEQUIPMENTTYPE_MEGAPOL_LEG_ARMOR", "AEQUIPMENTTYPE_MEGAPOL_BODY_ARMOR",
	                 "AEQUIPMENTTYPE_MEGAPOL_RIGHT_ARM_ARMOR",
	                 "AEQUIPMENTTYPE_MEGAPOL_LEFT_ARM_ARMOR", "AEQUIPMENTTYPE_MEGAPOL_HELMET"},
	                "Megapol C"))
	{
		return false;
	}

	auto marsec = state.organisations.find("ORG_MARSEC");
	TEST_REQUIRE(marsec != state.organisations.end() && marsec->second, "ORG_MARSEC missing");
	if (!expectLoot(*marsec->second, Organisation::LootPriority::A,
	                {"AEQUIPMENTTYPE_MARSEC_PROXIMITY_MINE", "AEQUIPMENTTYPE_MARSEC_HIGH_EXPLOSIVE",
	                 "AEQUIPMENTTYPE_MARSEC_M4000_MACHINE_GUN",
	                 "AEQUIPMENTTYPE_MARSEC_HEAVY_LAUNCHER", "AEQUIPMENTTYPE_MARSEC_MINILAUNCHER"},
	                "Marsec A"))
	{
		return false;
	}

	auto civilian = state.organisations.find("ORG_CIVILIAN");
	TEST_REQUIRE(civilian != state.organisations.end() && civilian->second, "ORG_CIVILIAN missing");
	for (const auto priority : {Organisation::LootPriority::A, Organisation::LootPriority::B,
	                            Organisation::LootPriority::C})
	{
		for (const auto &item : lootIds(*civilian->second, priority))
		{
			TEST_REQUIRE(item.empty(), "civilian loot slot {0}", item);
		}
	}
	return true;
}

static bool test_org_extracted_scalars()
{
	auto &state = *g_state;
	// UFO2P non-4 organisation_data at 0x141468 (18 B/record).
	// raiding_strength +2 uint32, average_guards +7 uint8, rebuilding_rate +16 uint16.
	auto megapol = state.organisations.find("ORG_MEGAPOL");
	TEST_REQUIRE(megapol != state.organisations.end() && megapol->second, "ORG_MEGAPOL missing");
	TEST_REQUIRE(megapol->second->raidingStrength == 5000, "Megapol raidingStrength {0}",
	             megapol->second->raidingStrength);
	TEST_REQUIRE(megapol->second->rebuildingRate == 15, "Megapol rebuildingRate {0}",
	             megapol->second->rebuildingRate);
	TEST_REQUIRE(megapol->second->average_guards == 16, "Megapol average_guards {0}",
	             megapol->second->average_guards);

	auto marsec = state.organisations.find("ORG_MARSEC");
	TEST_REQUIRE(marsec != state.organisations.end() && marsec->second, "ORG_MARSEC missing");
	TEST_REQUIRE(marsec->second->raidingStrength == 4000, "Marsec raidingStrength {0}",
	             marsec->second->raidingStrength);
	TEST_REQUIRE(marsec->second->rebuildingRate == 15, "Marsec rebuildingRate {0}",
	             marsec->second->rebuildingRate);
	TEST_REQUIRE(marsec->second->average_guards == 14, "Marsec average_guards {0}",
	             marsec->second->average_guards);
	return true;
}

static bool test_cequip_score_req()
{
	auto &state = *g_state;
	// UFO2P non-4 cequip_score_req_data at 0x1421C4: 5×5 uint32, rows 0-3 = vequip 44-47.
	static const char *ids[4] = {"VEQUIPMENTTYPE_SMALL_DISRUPTION_SHIELD",
	                             "VEQUIPMENTTYPE_LARGE_DISRUPTION_SHIELD",
	                             "VEQUIPMENTTYPE_CLOAKING_FIELD", "VEQUIPMENTTYPE_TELEPORTER"};
	static const int scores[4][5] = {
	    {2000, 1750, 1500, 1250, 1000},
	    {4000, 3500, 3000, 2500, 2000},
	    {8000, 7000, 6000, 5000, 4000},
	    {16000, 14000, 12000, 10000, 8000},
	};
	for (int row = 0; row < 4; row++)
	{
		auto it = state.vehicle_equipment.find(ids[row]);
		TEST_REQUIRE(it != state.vehicle_equipment.end() && it->second, "{0} missing", ids[row]);
		TEST_REQUIRE(it->second->scoreRequirementByDifficulty.size() == 5, "{0} score columns {1}",
		             ids[row], it->second->scoreRequirementByDifficulty.size());
		for (int d = 0; d < 5; d++)
		{
			TEST_REQUIRE(it->second->scoreRequirementByDifficulty[d] == scores[row][d],
			             "{0} difficulty {1} is {2}, expected {3}", ids[row], d,
			             it->second->scoreRequirementByDifficulty[d], scores[row][d]);
		}
		TEST_REQUIRE(it->second->scoreRequirement == scores[row][0], "{0} scoreRequirement {1}",
		             ids[row], it->second->scoreRequirement);
		TEST_REQUIRE(it->second->scoreRequirementFor(0) == scores[row][0], "{0} novice gate",
		             ids[row]);
		TEST_REQUIRE(it->second->scoreRequirementFor(4) == scores[row][4], "{0} superhuman gate",
		             ids[row]);
	}
	auto shifter = state.vehicle_equipment.find("VEQUIPMENTTYPE_DIMENSION_SHIFTER");
	TEST_REQUIRE(shifter != state.vehicle_equipment.end() && shifter->second,
	             "DIMENSION_SHIFTER missing");
	TEST_REQUIRE(shifter->second->scoreRequirement == 0, "row 4 must not bind Dimension Shifter");
	TEST_REQUIRE(shifter->second->scoreRequirementByDifficulty.empty(),
	             "Dimension Shifter must not take the unnamed fifth row");

	StateRef<VehicleType> scout{&state, "VEHICLETYPE_ALIEN_SCOUT"};
	TEST_REQUIRE(!!scout, "ALIEN_SCOUT missing");
	bool scoutHasShield = false;
	for (auto &pair : scout->initial_equipment_list)
	{
		if (pair.second.id == ids[0])
		{
			scoutHasShield = true;
			break;
		}
	}
	TEST_REQUIRE(scoutHasShield, "scout default loadout missing small shield");

	auto cityIt = state.cities.find("CITYMAP_HUMAN");
	TEST_REQUIRE(cityIt != state.cities.end() && cityIt->second, "CITYMAP_HUMAN missing");
	const auto savedScore = state.totalScore.craftShotDownUFO;
	auto probe = cityIt->second->createVehicle(state, scout, state.getAliens());
	TEST_REQUIRE(!!probe, "failed to create alien scout");
	state.totalScore.craftShotDownUFO = 1999;
	probe->equipDefaultEquipment(state);
	bool skipped = true;
	for (auto &e : probe->loot)
	{
		if (e.id == ids[0])
		{
			skipped = false;
			break;
		}
	}
	state.totalScore.craftShotDownUFO = 2000;
	probe->equipDefaultEquipment(state);
	bool allowed = false;
	for (auto &e : probe->loot)
	{
		if (e.id == ids[0])
		{
			allowed = true;
			break;
		}
	}
	state.totalScore.craftShotDownUFO = savedScore;
	UString eraseId;
	for (auto &p : state.vehicles)
	{
		if (p.second == probe)
		{
			eraseId = p.first;
			break;
		}
	}
	if (!eraseId.empty())
	{
		state.vehicles.erase(eraseId);
	}
	TEST_REQUIRE(skipped, "small shield equipped below 2000");
	TEST_REQUIRE(allowed, "small shield skipped at 2000");
	return true;
}

static bool test_aequip_artifact_and_resist()
{
	auto &state = *g_state;
	// UFO2P aequip_alien_artifact_data @ 0x1422A8. TACP unknown01 is fire resist.
	auto destab = state.agent_equipment.find("AEQUIPMENTTYPE_DIMENSION_DESTABILISER");
	auto disruptor = state.agent_equipment.find("AEQUIPMENTTYPE_DISRUPTOR_GUN");
	auto plasma = state.agent_equipment.find("AEQUIPMENTTYPE_MEGAPOL_PLASMA_GUN");
	auto megapol = state.agent_equipment.find("AEQUIPMENTTYPE_MEGAPOL_BODY_ARMOR");
	auto marsec = state.agent_equipment.find("AEQUIPMENTTYPE_MARSEC_BODY_UNIT");
	auto xcom = state.agent_equipment.find("AEQUIPMENTTYPE_X-COM_BODY_SHIELD");
	auto shield = state.agent_equipment.find("AEQUIPMENTTYPE_PERSONAL_DISRUPTOR_SHIELD");
	TEST_REQUIRE(destab != state.agent_equipment.end() && destab->second, "destabiliser missing");
	TEST_REQUIRE(disruptor != state.agent_equipment.end() && disruptor->second,
	             "disruptor missing");
	TEST_REQUIRE(plasma != state.agent_equipment.end() && plasma->second, "plasma gun missing");
	TEST_REQUIRE(megapol != state.agent_equipment.end() && megapol->second, "megapol body missing");
	TEST_REQUIRE(marsec != state.agent_equipment.end() && marsec->second, "marsec body missing");
	TEST_REQUIRE(xcom != state.agent_equipment.end() && xcom->second, "xcom body missing");
	TEST_REQUIRE(shield != state.agent_equipment.end() && shield->second,
	             "disruptor shield missing");
	TEST_REQUIRE(destab->second->artifact, "Dimension Destabiliser must be an artifact");
	TEST_REQUIRE(disruptor->second->artifact, "Disruptor Gun must be an artifact");
	TEST_REQUIRE(shield->second->artifact, "Personal Disruptor Shield must be an artifact");
	TEST_REQUIRE(!plasma->second->artifact, "Megapol Plasma Gun must not be an artifact");
	TEST_REQUIRE(!megapol->second->artifact, "Megapol Body Armor must not be an artifact");
	TEST_REQUIRE(megapol->second->hazardResist == 0, "Megapol armor resist {0}",
	             megapol->second->hazardResist);
	TEST_REQUIRE(marsec->second->hazardResist == 50, "Marsec armor resist {0}",
	             marsec->second->hazardResist);
	TEST_REQUIRE(xcom->second->hazardResist == 100, "X-COM armor resist {0}",
	             xcom->second->hazardResist);
	TEST_REQUIRE(shield->second->hazardResist == 200, "disruptor shield resist {0}",
	             shield->second->hazardResist);
	TEST_REQUIRE(AEquipmentType::fireHazardDamage(10, 50) == 1, "Marsec factor-1 still takes 1");
	TEST_REQUIRE(AEquipmentType::fireHazardDamage(10, 100) == 0, "X-COM factor-1 takes 0");
	TEST_REQUIRE(AEquipmentType::fireHazardDamage(10, 200) == -1, "resist 200 signed delta");
	const std::vector<int> expectedFirePower = {5,  10, 15, 20, 25, 30, 35, 40, 45,
	                                            50, 55, 60, 65, 70, 75, 70, 75, 70,
	                                            75, 70, 65, 55, 45, 35, 25, 15, 5};
	TEST_REQUIRE(state.fireHazardPowerTable == expectedFirePower,
	             "TACP fire power table differs from 0x2E2AF4");

	// FUN_000811fc hides artifacts; FUN_000ab440 clears on same-name type-1 complete.
	auto gun = state.research.topics.find("RESEARCH_DISRUPTOR_GUN");
	TEST_REQUIRE(gun != state.research.topics.end() && gun->second, "DISRUPTOR_GUN topic missing");
	TEST_REQUIRE(disruptor->second->name == gun->second->name, "name match {0} vs {1}",
	             disruptor->second->name, gun->second->name);
	const auto savedProgress = gun->second->man_hours_progress;
	const auto savedStarted = gun->second->started;
	gun->second->man_hours_progress = 0;
	TEST_REQUIRE(!disruptor->second->isEconomyVisible(), "incomplete artifact stays hidden");
	gun->second->forceComplete();
	TEST_REQUIRE(disruptor->second->isEconomyVisible(), "completed artifact is in economy");
	gun->second->man_hours_progress = savedProgress;
	gun->second->started = savedStarted;

	const bool savedArtifact = destab->second->artifact;
	const auto savedDep = destab->second->research_dependency;
	destab->second->artifact = true;
	destab->second->research_dependency = {};
	TEST_REQUIRE(destab->second->isResearched(), "empty dep uses satisfied() fallback");
	TEST_REQUIRE(!destab->second->isEconomyVisible(), "artifact empty dep is not in market");
	destab->second->artifact = savedArtifact;
	destab->second->research_dependency = savedDep;

	// FUN_000ab440: same-name type-1 complete shows artifact index 37.
	auto destTopic = state.research.topics.find("RESEARCH_DIMENSION_DESTABILISER");
	TEST_REQUIRE(destTopic != state.research.topics.end() && destTopic->second,
	             "DESTABILISER topic missing");
	TEST_REQUIRE(destab->second->name == destTopic->second->name, "destab name {0} vs {1}",
	             destab->second->name, destTopic->second->name);
	bool destabHasTopic = false;
	for (auto &t : destab->second->research_dependency.topics)
	{
		if (t.id == "RESEARCH_DIMENSION_DESTABILISER")
		{
			destabHasTopic = true;
		}
	}
	TEST_REQUIRE(destabHasTopic, "destab lost RESEARCH_DIMENSION_DESTABILISER");
	const auto destProgress = destTopic->second->man_hours_progress;
	const auto destStarted = destTopic->second->started;
	destTopic->second->man_hours_progress = 0;
	TEST_REQUIRE(!destab->second->isEconomyVisible(), "incomplete destab stays hidden");
	destTopic->second->forceComplete();
	TEST_REQUIRE(destab->second->isEconomyVisible(), "completed destab is in economy");
	destTopic->second->man_hours_progress = destProgress;
	destTopic->second->started = destStarted;

	// FUN_000ab440 case 1 @ file 0x10DC3C / 4-build 0x10E4AE: manufacture
	// itemIndex (not strcmp) clears DAT_00183b3b. Indices 19/23 have no
	// same-name type-1 topic; techRequired 42 is Alien Gas.
	auto heavy = state.agent_equipment.find("AEQUIPMENTTYPE_HEAVY_LAUNCHER_AG_MISSILE");
	auto mini = state.agent_equipment.find("AEQUIPMENTTYPE_MINILAUNCHER_AG_MISSILE");
	auto pod = state.agent_equipment.find("AEQUIPMENTTYPE_BRAINSUCKER_POD");
	auto heavyMfg = state.research.topics.find("MANUFACTURE_HEAVY_LAUNCHER_ALIEN_GAS_MISSILE");
	auto miniMfg = state.research.topics.find("MANUFACTURE_MINI_LAUNCHER_ALIEN_GAS_MISSILE");
	auto gas = state.research.topics.find("RESEARCH_ALIEN_GAS");
	TEST_REQUIRE(heavy != state.agent_equipment.end() && heavy->second, "heavy AG missing");
	TEST_REQUIRE(mini != state.agent_equipment.end() && mini->second, "mini AG missing");
	TEST_REQUIRE(pod != state.agent_equipment.end() && pod->second, "brainsucker pod missing");
	TEST_REQUIRE(heavyMfg != state.research.topics.end() && heavyMfg->second, "heavy mfg missing");
	TEST_REQUIRE(miniMfg != state.research.topics.end() && miniMfg->second, "mini mfg missing");
	TEST_REQUIRE(gas != state.research.topics.end() && gas->second, "ALIEN_GAS missing");
	TEST_REQUIRE(heavy->second->artifact && mini->second->artifact,
	             "AG missiles must be artifacts");
	TEST_REQUIRE(pod->second->artifact, "Brainsucker Pod must stay an artifact");
	TEST_REQUIRE(heavyMfg->second->itemId == "AEQUIPMENTTYPE_HEAVY_LAUNCHER_AG_MISSILE",
	             "heavy mfg itemIndex 19");
	TEST_REQUIRE(miniMfg->second->itemId == "AEQUIPMENTTYPE_MINILAUNCHER_AG_MISSILE",
	             "mini mfg itemIndex 23");
	TEST_REQUIRE(
	    hasExactDep(*heavyMfg->second, ResearchDependency::Type::All, {"RESEARCH_ALIEN_GAS"}),
	    "heavy mfg lost techRequired 42");
	TEST_REQUIRE(
	    hasExactDep(*miniMfg->second, ResearchDependency::Type::All, {"RESEARCH_ALIEN_GAS"}),
	    "mini mfg lost techRequired 42");
	const auto gasProgress = gas->second->man_hours_progress;
	const auto gasStarted = gas->second->started;
	const bool savedHeavyUnhide = heavy->second->artifactUnhidden;
	const bool savedMiniUnhide = mini->second->artifactUnhidden;
	heavy->second->artifactUnhidden = false;
	mini->second->artifactUnhidden = false;
	gas->second->man_hours_progress = 0;
	TEST_REQUIRE(!heavy->second->isEconomyVisible() && !mini->second->isEconomyVisible(),
	             "AG missiles stay hidden before manufacture");
	gas->second->forceComplete();
	TEST_REQUIRE(!heavy->second->isEconomyVisible(),
	             "Alien Gas research must not unhide Heavy AG Missile");
	TEST_REQUIRE(!mini->second->isEconomyVisible(),
	             "Alien Gas research must not unhide Mini AG Missile");
	TEST_REQUIRE(!pod->second->isEconomyVisible(), "index 57 is never cleared");
	heavy->second->clearEconomyHide();
	TEST_REQUIRE(heavy->second->isEconomyVisible(),
	             "manufacture itemIndex 19 must unhide Heavy AG Missile");
	TEST_REQUIRE(!mini->second->isEconomyVisible(), "clearing 19 must not unhide 23");
	mini->second->clearEconomyHide();
	TEST_REQUIRE(mini->second->isEconomyVisible(),
	             "manufacture itemIndex 23 must unhide Mini AG Missile");
	TEST_REQUIRE(!pod->second->isEconomyVisible(), "index 57 stays hidden after AG manufacture");
	heavy->second->artifactUnhidden = savedHeavyUnhide;
	mini->second->artifactUnhidden = savedMiniUnhide;
	gas->second->man_hours_progress = gasProgress;
	gas->second->started = gasStarted;
	TEST_REQUIRE(plasma->second->isEconomyVisible() == plasma->second->isResearched(),
	             "non-artifact economy follows isResearched");
	return true;
}

static bool test_aequip_market_week0()
{
	auto &state = *g_state;
	// FUN_000811fc @ file 0xE3971 / 4-build 0xE39BC: shop hide is
	// DAT_00183b3b only. economy_data3 week is 0 for AG 19/23, destab 37.
	auto destab = state.agent_equipment.find("AEQUIPMENTTYPE_DIMENSION_DESTABILISER");
	auto heavy = state.agent_equipment.find("AEQUIPMENTTYPE_HEAVY_LAUNCHER_AG_MISSILE");
	auto pod = state.agent_equipment.find("AEQUIPMENTTYPE_BRAINSUCKER_POD");
	auto destTopic = state.research.topics.find("RESEARCH_DIMENSION_DESTABILISER");
	TEST_REQUIRE(destab != state.agent_equipment.end() && destab->second, "destab missing");
	TEST_REQUIRE(heavy != state.agent_equipment.end() && heavy->second, "heavy AG missing");
	TEST_REQUIRE(pod != state.agent_equipment.end() && pod->second, "pod missing");
	TEST_REQUIRE(destTopic != state.research.topics.end() && destTopic->second,
	             "destab topic missing");
	auto destabEco = state.economy.find(destab->second->id);
	auto heavyEco = state.economy.find(heavy->second->id);
	auto podEco = state.economy.find(pod->second->id);
	TEST_REQUIRE(destabEco != state.economy.end() && destabEco->second.weekAvailable == 0,
	             "destab week must be 0");
	TEST_REQUIRE(heavyEco != state.economy.end() && heavyEco->second.weekAvailable == 0,
	             "heavy AG week must be 0");
	TEST_REQUIRE(podEco != state.economy.end() && podEco->second.weekAvailable == 0,
	             "pod week must be 0");

	const auto destProgress = destTopic->second->man_hours_progress;
	const auto destStarted = destTopic->second->started;
	const bool savedHeavyUnhide = heavy->second->artifactUnhidden;
	destTopic->second->man_hours_progress = 0;
	destTopic->second->started = false;
	heavy->second->artifactUnhidden = false;
	TEST_REQUIRE(!destab->second->isMarketListed(state), "incomplete destab stays off market");
	destTopic->second->forceComplete();
	TEST_REQUIRE(destab->second->isEconomyVisible(), "same-name destab research unhides");
	TEST_REQUIRE(destab->second->isMarketListed(state),
	             "week 0 must not hide destab after FUN_000aac88");
	TEST_REQUIRE(!heavy->second->isMarketListed(state), "AG stays hidden until manufacture");
	heavy->second->clearEconomyHide();
	TEST_REQUIRE(heavy->second->isMarketListed(state),
	             "week 0 must not hide AG after FUN_000ab440 case 1");
	TEST_REQUIRE(!pod->second->isMarketListed(state), "index 57 never lists");
	heavy->second->artifactUnhidden = savedHeavyUnhide;
	destTopic->second->man_hours_progress = destProgress;
	destTopic->second->started = destStarted;
	return true;
}

static bool test_vehicle_park_spawn()
{
	auto &state = *g_state;
	// UFO2P vehicle_park_spawn_table @ 0x188F18 (40 IDs); caps @ 0x188FB8.
	TEST_REQUIRE(state.vehicleParkSpawnTable.size() == 40, "spawn table size {0}",
	             state.vehicleParkSpawnTable.size());
	TEST_REQUIRE(state.vehicleParkSpawnTable[0].id == "VEHICLETYPE_PHOENIX_HOVERCAR",
	             "civilian pool starts with Phoenix");
	TEST_REQUIRE(state.vehicleParkSpawnTable[20].id == "VEHICLETYPE_POLICE_HOVERCAR",
	             "Megapol pool starts with Police Hover");
	TEST_REQUIRE(state.vehicleParkSpawnTable[39].id == "VEHICLETYPE_HAWK_AIR_WARRIOR",
	             "index 39 is the dword FUN_000962cc reads past 0–38");
	TEST_REQUIRE(state.vehicleParkSpawnCap["VEHICLETYPE_PHOENIX_HOVERCAR"] == 7, "Phoenix cap");
	TEST_REQUIRE(state.vehicleParkSpawnCap["VEHICLETYPE_HAWK_AIR_WARRIOR"] == 2, "Hawk cap");
	TEST_REQUIRE(state.vehicleParkSpawnCap["VEHICLETYPE_GRIFFON_AFV"] == 1, "Griffon cap");
	TEST_REQUIRE(state.vehicleParkSpawnCap["VEHICLETYPE_POLICE_HOVERCAR"] == 15,
	             "Police Hover cap");
	auto mega = state.organisations.find("ORG_MEGAPOL");
	TEST_REQUIRE(mega != state.organisations.end() && mega->second, "ORG_MEGAPOL missing");
	TEST_REQUIRE(mega->second->exeOrgIndex == 3, "Megapol exe index {0}",
	             mega->second->exeOrgIndex);
	TEST_REQUIRE(mega->second->parkBudgetWeight == 55, "Megapol park scalar {0}",
	             mega->second->parkBudgetWeight);
	auto nutr = state.organisations.find("ORG_NUTRIVEND");
	TEST_REQUIRE(nutr != state.organisations.end() && nutr->second, "ORG_NUTRIVEND missing");
	TEST_REQUIRE(nutr->second->exeOrgIndex == 13, "Nutrivend exe index {0}",
	             nutr->second->exeOrgIndex);
	TEST_REQUIRE(nutr->second->parkBudgetWeight == 2, "Nutrivend park scalar {0}",
	             nutr->second->parkBudgetWeight);
	auto saved = nutr->second->current_relations;
	const int savedBalance = nutr->second->balance;
	nutr->second->current_relations.clear();
	TEST_REQUIRE(nutr->second->parkHostileWeight(state) == 0, "cleared relations still hostile");
	StateRef<Organisation> other{&state, "ORG_MEGAPOL"};
	nutr->second->current_relations[other] = -50.0f;
	TEST_REQUIRE(nutr->second->parkHostileWeight(state) == 2, "rel -50 must add 2");
	nutr->second->current_relations[other] = -49.0f;
	TEST_REQUIRE(nutr->second->parkHostileWeight(state) == 0, "rel -49 is not < -0x31");
	nutr->second->balance = 100000;
	nutr->second->current_relations.clear();
	TEST_REQUIRE(nutr->second->parkPurchaseBudget(state) ==
	                 (100000 * nutr->second->parkBudgetWeight) / 100,
	             "budget is funds * (hostile + parkScalar) / 100");
	nutr->second->current_relations = saved;
	nutr->second->balance = savedBalance;
	return true;
}

static bool test_ufopaedia_start_visible()
{
	auto &state = *g_state;
	auto requireEntry = [&](const UString &id) -> sp<UfopaediaEntry>
	{
		auto it = state.ufopaedia_entries.find(id);
		if (it == state.ufopaedia_entries.end())
		{
			return nullptr;
		}
		return it->second;
	};

	auto shield = requireEntry("PAEDIAENTRY_XCOM_BODY_SHIELD");
	TEST_REQUIRE(shield, "PAEDIAENTRY_XCOM_BODY_SHIELD missing");
	TEST_REQUIRE(shield->startVisibleFromExe, "ARMOUR3 catalog row must bind Body Shield");
	TEST_REQUIRE(shield->startVisible, "ARMOUR3 start byte is 1");
	TEST_REQUIRE(shield->isVisible(state), "FUN_0008903c start byte shows Body Shield");
	TEST_REQUIRE(!shield->dependency.satisfied(), "Disruptor Armor research is not complete");

	auto inc = requireEntry("PAEDIAENTRY_INCENDIARY_GRENADE");
	TEST_REQUIRE(inc, "PAEDIAENTRY_INCENDIARY_GRENADE missing");
	TEST_REQUIRE(inc->startVisibleFromExe, "W59 catalog row must bind Incendiary Grenade");
	TEST_REQUIRE(!inc->startVisible, "W59 start byte is 0");
	TEST_REQUIRE(!inc->isVisible(state), "empty dep must not show a start-hidden page");

	auto destab = requireEntry("PAEDIAENTRY_DIMENSION_DESTABILISER");
	TEST_REQUIRE(destab, "PAEDIAENTRY_DIMENSION_DESTABILISER missing");
	TEST_REQUIRE(destab->startVisibleFromExe, "W37 catalog row must bind Destabiliser");
	TEST_REQUIRE(!destab->startVisible, "W37 start byte is 0");
	TEST_REQUIRE(!destab->isVisible(state), "Destabiliser stays research-gated");

	auto bio = requireEntry("PAEDIAENTRY_BIO_TRANSPORT_MODULE");
	TEST_REQUIRE(bio, "PAEDIAENTRY_BIO_TRANSPORT_MODULE missing");
	TEST_REQUIRE(!bio->startVisible, "V32 start byte is 0");
	TEST_REQUIRE(!bio->isVisible(state), "Bio-Transport Module stays research-gated");

	auto lab = requireEntry("PAEDIAENTRY_BIOCHEMISTRY_LAB");
	TEST_REQUIRE(lab, "PAEDIAENTRY_BIOCHEMISTRY_LAB missing");
	TEST_REQUIRE(lab->startVisible && lab->isVisible(state), "Biochemistry Lab starts visible");

	auto warehouse = requireEntry("PAEDIAENTRY_WAREHOUSE");
	TEST_REQUIRE(warehouse, "PAEDIAENTRY_WAREHOUSE missing");
	TEST_REQUIRE(!warehouse->startVisibleFromExe, "30warehs is not in the EXE catalog");
	TEST_REQUIRE(warehouse->isVisible(state), "unmapped empty-dep building stays visible");

	auto genetics = requireEntry("PAEDIAENTRY_THE_ALIEN_GENETIC_STRUCTURE");
	TEST_REQUIRE(genetics, "PAEDIAENTRY_THE_ALIEN_GENETIC_STRUCTURE missing");
	TEST_REQUIRE(!genetics->startVisibleFromExe,
	             "genetics.pcx building row must not bind the research page");
	TEST_REQUIRE(!genetics->isVisible(state), "Genetic Structure stays research-gated");
	return true;
}

static bool test_ufopaedia_abf9c_unlock()
{
	auto &state = *g_state;
	// FUN_000abf9c @ file 0xFE640: research_data (group, entry) sets the
	// catalog start byte. Extra cat 3 0x1B → 0x13 + 0x17.
	auto incubator = state.ufopaedia_entries.find("PAEDIAENTRY_INCUBATOR_CHAMBER");
	auto sleeping = state.ufopaedia_entries.find("PAEDIAENTRY_SLEEPING_CHAMBER");
	auto alienDim = state.ufopaedia_entries.find("PAEDIAENTRY_THE_ALIEN_DIMENSION");
	auto gatesPage = state.ufopaedia_entries.find("PAEDIAENTRY_DIMENSION_GATES");
	auto building0 = state.research.topics.find("RESEARCH_ALIEN_BUILDING_0");
	auto gates = state.research.topics.find("RESEARCH_DIMENSION_GATES");
	TEST_REQUIRE(incubator != state.ufopaedia_entries.end() && incubator->second,
	             "incubator paedia missing");
	TEST_REQUIRE(sleeping != state.ufopaedia_entries.end() && sleeping->second,
	             "sleeping paedia missing");
	TEST_REQUIRE(alienDim != state.ufopaedia_entries.end() && alienDim->second,
	             "alien dimension paedia missing");
	TEST_REQUIRE(gatesPage != state.ufopaedia_entries.end() && gatesPage->second,
	             "dimension gates paedia missing");
	TEST_REQUIRE(building0 != state.research.topics.end() && building0->second,
	             "ALIEN_BUILDING_0 missing");
	TEST_REQUIRE(gates != state.research.topics.end() && gates->second, "DIMENSION_GATES missing");
	TEST_REQUIRE(building0->second->ufopaediaGroup == 8 && building0->second->ufopaediaEntry == 0,
	             "BUILDING_0 catalog {0},{1}", building0->second->ufopaediaGroup,
	             building0->second->ufopaediaEntry);
	TEST_REQUIRE(gates->second->ufopaediaGroup == 8 && gates->second->ufopaediaEntry == 10,
	             "GATES catalog {0},{1}", gates->second->ufopaediaGroup,
	             gates->second->ufopaediaEntry);
	TEST_REQUIRE(incubator->second->catalogCategory == 8 && incubator->second->catalogIndex == 0,
	             "incubator bind {0},{1}", incubator->second->catalogCategory,
	             incubator->second->catalogIndex);
	TEST_REQUIRE(sleeping->second->catalogCategory == 8 && sleeping->second->catalogIndex == 4,
	             "sleeping bind {0},{1}", sleeping->second->catalogCategory,
	             sleeping->second->catalogIndex);
	TEST_REQUIRE(alienDim->second->catalogCategory == 8 && alienDim->second->catalogIndex == 10,
	             "alien dimension bind {0},{1}", alienDim->second->catalogCategory,
	             alienDim->second->catalogIndex);

	const auto b0Progress = building0->second->man_hours_progress;
	const auto b0Started = building0->second->started;
	const auto gatesProgress = gates->second->man_hours_progress;
	const auto gatesStarted = gates->second->started;
	const bool savedInc = incubator->second->startVisible;
	const bool savedSleep = sleeping->second->startVisible;
	const bool savedDim = alienDim->second->startVisible;
	const bool savedGates = gatesPage->second->startVisible;
	building0->second->man_hours_progress = 0;
	building0->second->started = false;
	gates->second->man_hours_progress = 0;
	gates->second->started = false;
	incubator->second->startVisible = false;
	sleeping->second->startVisible = false;
	alienDim->second->startVisible = false;
	gatesPage->second->startVisible = false;

	TEST_REQUIRE(!incubator->second->isVisible(state), "incubator starts hidden");
	building0->second->forceComplete(&state);
	TEST_REQUIRE(incubator->second->startVisible, "FUN_000abf9c must set incubator start byte");
	TEST_REQUIRE(incubator->second->isVisible(state), "BUILDING_0 shows Incubator");
	TEST_REQUIRE(!sleeping->second->startVisible && !sleeping->second->isVisible(state),
	             "BUILDING_0 must not show Sleeping (catalog 8,4)");

	TEST_REQUIRE(!alienDim->second->isVisible(state), "Alien Dimension starts hidden");
	gates->second->forceComplete(&state);
	TEST_REQUIRE(alienDim->second->startVisible, "FUN_000abf9c must set 11dimens start byte");
	TEST_REQUIRE(alienDim->second->isVisible(state), "Dimension Gates research shows 11dimens");
	TEST_REQUIRE(!gatesPage->second->startVisible, "Gates page is catalog 6,32 not 8,10");

	incubator->second->startVisible = savedInc;
	sleeping->second->startVisible = savedSleep;
	alienDim->second->startVisible = savedDim;
	gatesPage->second->startVisible = savedGates;
	building0->second->man_hours_progress = b0Progress;
	building0->second->started = b0Started;
	gates->second->man_hours_progress = gatesProgress;
	gates->second->started = gatesStarted;
	return true;
}

static bool test_ufopaedia_economy_hide()
{
	auto &state = *g_state;
	// FUN_0008c860 @ file 0xEEF04 (4-build case 2/3 @ 0xEF2C8 / 0xEF2D6):
	// catalog type 2 reads DAT_00183b3b, type 3 reads DAT_00183b0a.
	auto heavyPage = state.ufopaedia_entries.find("PAEDIAENTRY_HEAVY_LAUNCHER_AG_MISSILE");
	auto miniPage = state.ufopaedia_entries.find("PAEDIAENTRY_MINILAUNCHER_AG_MISSILE");
	auto shieldPage = state.ufopaedia_entries.find("PAEDIAENTRY_SMALL_DISRUPTION_SHIELD");
	auto warehouse = state.ufopaedia_entries.find("PAEDIAENTRY_WAREHOUSE");
	auto heavy = state.agent_equipment.find("AEQUIPMENTTYPE_HEAVY_LAUNCHER_AG_MISSILE");
	auto mini = state.agent_equipment.find("AEQUIPMENTTYPE_MINILAUNCHER_AG_MISSILE");
	auto shield = state.vehicle_equipment.find("VEQUIPMENTTYPE_SMALL_DISRUPTION_SHIELD");
	auto gas = state.research.topics.find("RESEARCH_ALIEN_GAS");
	auto shieldTopic = state.research.topics.find("RESEARCH_SMALL_DISRUPTION_SHIELD");
	TEST_REQUIRE(heavyPage != state.ufopaedia_entries.end() && heavyPage->second,
	             "heavy AG paedia missing");
	TEST_REQUIRE(miniPage != state.ufopaedia_entries.end() && miniPage->second,
	             "mini AG paedia missing");
	TEST_REQUIRE(shieldPage != state.ufopaedia_entries.end() && shieldPage->second,
	             "shield paedia missing");
	TEST_REQUIRE(warehouse != state.ufopaedia_entries.end() && warehouse->second,
	             "warehouse paedia missing");
	TEST_REQUIRE(heavy != state.agent_equipment.end() && heavy->second, "heavy AG missing");
	TEST_REQUIRE(mini != state.agent_equipment.end() && mini->second, "mini AG missing");
	TEST_REQUIRE(shield != state.vehicle_equipment.end() && shield->second, "small shield missing");
	TEST_REQUIRE(gas != state.research.topics.end() && gas->second, "ALIEN_GAS missing");
	TEST_REQUIRE(shieldTopic != state.research.topics.end() && shieldTopic->second,
	             "shield research missing");

	const auto gasProgress = gas->second->man_hours_progress;
	const auto gasStarted = gas->second->started;
	const auto shieldProgress = shieldTopic->second->man_hours_progress;
	const auto shieldStarted = shieldTopic->second->started;
	const bool savedHeavyUnhide = heavy->second->artifactUnhidden;
	const bool savedMiniUnhide = mini->second->artifactUnhidden;
	const bool savedShieldUnhide = shield->second->economyUnhidden;
	heavy->second->artifactUnhidden = false;
	mini->second->artifactUnhidden = false;
	shield->second->economyUnhidden = false;
	gas->second->man_hours_progress = 0;
	gas->second->started = false;
	shieldTopic->second->man_hours_progress = 0;
	shieldTopic->second->started = false;

	TEST_REQUIRE(warehouse->second->isVisible(state), "Warehouse is not hide-gated");
	gas->second->forceComplete(&state);
	TEST_REQUIRE(!heavyPage->second->isVisible(state) && !miniPage->second->isVisible(state),
	             "FUN_0008c860 case 2 keeps AG pages hidden after Alien Gas");
	heavy->second->clearEconomyHide();
	mini->second->clearEconomyHide();
	TEST_REQUIRE(heavyPage->second->isVisible(state) && miniPage->second->isVisible(state),
	             "type-1 manufacture must show AG paedia");

	shieldTopic->second->forceComplete(&state);
	TEST_REQUIRE(!shieldPage->second->isVisible(state),
	             "FUN_0008c860 case 3 keeps Small Shield paedia hidden after research");
	shield->second->clearEconomyHide();
	TEST_REQUIRE(shieldPage->second->isVisible(state),
	             "type-0 manufacture must show Small Shield paedia");

	heavy->second->artifactUnhidden = savedHeavyUnhide;
	mini->second->artifactUnhidden = savedMiniUnhide;
	shield->second->economyUnhidden = savedShieldUnhide;
	gas->second->man_hours_progress = gasProgress;
	gas->second->started = gasStarted;
	shieldTopic->second->man_hours_progress = shieldProgress;
	shieldTopic->second->started = shieldStarted;
	return true;
}

static bool test_vequip_economy_hide()
{
	auto &state = *g_state;
	// FUN_00014854 @ file 0x77116: DAT_00183b0a[i] = (economy week == 0), 49
	// entries. FUN_000ab440 case 0 @ file 0x10DC0D / 4-build 0x10E481 clears
	// hide[itemIndex]. Small Disruption Shield is vequip 44, mfg rec 16.
	auto shield = state.vehicle_equipment.find("VEQUIPMENTTYPE_SMALL_DISRUPTION_SHIELD");
	auto bolter = state.vehicle_equipment.find("VEQUIPMENTTYPE_BOLTER_4000_LASER_GUN");
	auto mfg = state.research.topics.find("MANUFACTURE_SMALL_DISRUPTION_SHIELD");
	auto topic = state.research.topics.find("RESEARCH_SMALL_DISRUPTION_SHIELD");
	TEST_REQUIRE(shield != state.vehicle_equipment.end() && shield->second, "small shield missing");
	TEST_REQUIRE(bolter != state.vehicle_equipment.end() && bolter->second, "bolter missing");
	TEST_REQUIRE(mfg != state.research.topics.end() && mfg->second, "shield mfg missing");
	TEST_REQUIRE(topic != state.research.topics.end() && topic->second, "shield research missing");
	TEST_REQUIRE(mfg->second->itemId == "VEQUIPMENTTYPE_SMALL_DISRUPTION_SHIELD",
	             "mfg itemIndex 44");
	TEST_REQUIRE(mfg->second->item_type == ResearchTopic::ItemType::VehicleEquipment, "mfg type 0");
	auto shieldEco = state.economy.find(shield->second->id);
	auto bolterEco = state.economy.find(bolter->second->id);
	TEST_REQUIRE(shieldEco != state.economy.end(), "small shield economy missing");
	TEST_REQUIRE(bolterEco != state.economy.end(), "bolter economy missing");
	TEST_REQUIRE(shieldEco->second.weekAvailable == 0, "shield week must be 0");
	TEST_REQUIRE(bolterEco->second.weekAvailable == 1, "bolter week must be 1");

	const bool savedUnhide = shield->second->economyUnhidden;
	const bool savedBolterUnhide = bolter->second->economyUnhidden;
	const auto savedProgress = topic->second->man_hours_progress;
	const auto savedStarted = topic->second->started;
	shield->second->economyUnhidden = false;
	bolter->second->economyUnhidden = false;
	topic->second->man_hours_progress = 0;
	topic->second->started = false;
	TEST_REQUIRE(!shield->second->isEconomyVisible(state), "week 0 stays hidden");
	topic->second->forceComplete();
	TEST_REQUIRE(!shield->second->isEconomyVisible(state),
	             "research complete must not clear DAT_00183b0a");
	shield->second->clearEconomyHide();
	TEST_REQUIRE(shield->second->isEconomyVisible(state),
	             "type-0 manufacture must unhide Small Disruption Shield");
	TEST_REQUIRE(bolter->second->isEconomyVisible(state) ==
	                 bolter->second->research_dependency.satisfied(),
	             "week 1 must not need manufacture unhide");
	shield->second->economyUnhidden = savedUnhide;
	bolter->second->economyUnhidden = savedBolterUnhide;
	topic->second->man_hours_progress = savedProgress;
	topic->second->started = savedStarted;
	return true;
}

static bool test_base_destroy_facility_errors()
{
	auto &state = *g_state;
	TEST_REQUIRE(!state.player_bases.empty(), "no player base");
	StateRef<Base> base{&state, state.player_bases.begin()->first};

	// Genuinely off-grid coordinates are OutOfBounds.
	TEST_REQUIRE(base->canDestroyFacility(state, {-1, 0}) == Base::BuildError::OutOfBounds,
	             "negative x is OutOfBounds");
	TEST_REQUIRE(base->canDestroyFacility(state, {0, Base::SIZE}) == Base::BuildError::OutOfBounds,
	             "y past the grid is OutOfBounds");

	// An in-range tile that simply holds nothing is NoFacility, not OutOfBounds. Conflating the
	// two made the startup cleanup sweep log a warning for every empty tile.
	bool sawEmpty = false;
	for (int y = 0; y < Base::SIZE && !sawEmpty; y++)
	{
		for (int x = 0; x < Base::SIZE; x++)
		{
			if (base->getFacility({x, y}))
			{
				continue;
			}
			TEST_REQUIRE(base->canDestroyFacility(state, {x, y}) == Base::BuildError::NoFacility,
			             "empty in-range tile is NoFacility");
			sawEmpty = true;
			break;
		}
	}
	TEST_REQUIRE(sawEmpty, "expected at least one empty tile in the starting base");

	// The access lift is fixed and must stay Indestructible.
	bool sawFixed = false;
	for (const auto &f : base->facilities)
	{
		if (f->type->fixed)
		{
			sawFixed = true;
			TEST_REQUIRE(base->canDestroyFacility(state, f->pos) ==
			                 Base::BuildError::Indestructible,
			             "fixed facility is Indestructible");
			break;
		}
	}
	TEST_REQUIRE(sawFixed, "starting base has no fixed facility");
	return true;
}

// The ten alien-dimension endgame maps, in InitialGameStateExtractor::battleMapPaths /
// missionObjectives order. TACP 0x2E0C09-0x2E1C98 packs one briefing per map in this order.
static const char *const ALIEN_BUILDING_MAPS[] = {
    "39incub", "40spawn", "41food",   "42megapd", "43sleep",
    "44organ", "45farm",  "46contrl", "47maint",  "48gate",
};

static bool test_alien_building_briefings_extracted()
{
	auto &state = *g_state;
	for (const auto *mapName : ALIEN_BUILDING_MAPS)
	{
		UString id = BattleMap::getPrefix() + mapName;
		auto it = state.battle_maps.find(id);
		TEST_REQUIRE(it != state.battle_maps.end() && it->second, "{0} battle map missing", id);
		TEST_REQUIRE(!it->second->briefing.empty(), "{0} has no briefing text extracted", id);
	}
	return true;
}

// Guards the destroy-objective mechanic (Battle::tryDisableBuilding /
// extract_battlescape_map_parts.cpp's missionObjective flag) against a future extractor edit
// silently dropping a building's objective set. The mechanic itself is not new: this is a
// freeze, matching A2/A3 in the parity guide - it is expected to pass both before and after
// the briefing-text change, and its teeth were confirmed by temporarily emptying one entry of
// InitialGameStateExtractor::missionObjectives and re-running extraction (see report).
static bool test_alien_building_objectives_present()
{
	auto &state = *g_state;

	// 40spawn's objective is the Queenspawn unit, not scenery: it ships an empty map-part
	// objective set by design (extract_agent_types.cpp sets AgentType::missionObjective on
	// AGENTTYPE_QUEENSPAWN instead).
	auto queen = state.agent_types.find("AGENTTYPE_QUEENSPAWN");
	TEST_REQUIRE(queen != state.agent_types.end() && queen->second, "AGENTTYPE_QUEENSPAWN missing");
	TEST_REQUIRE(queen->second->missionObjective, "40spawn objective (Queenspawn unit) missing");

	for (const auto *mapName : ALIEN_BUILDING_MAPS)
	{
		UString id = BattleMap::getPrefix() + mapName;
		auto it = state.battle_maps.find(id);
		TEST_REQUIRE(it != state.battle_maps.end() && it->second, "{0} battle map missing", id);

		if (UString(mapName) == "40spawn")
		{
			continue;
		}

		// Read the tileset directly (BattleMapTileset::loadTileset is a plain deserialize of
		// tileset.xml - it does not touch state.battleMapTiles or battle_common_sample_list,
		// unlike BattleMap::loadTilesets, which is private/friend-Battle-only and does more
		// than this check needs).
		BattleMapTileset tileset;
		UString tilesetPath = BattleMapTileset::getTilesetPath() + "/" + mapName;
		TEST_REQUIRE(tileset.loadTileset(state, tilesetPath), "{0} tileset failed to load",
		             mapName);
		bool hasObjective = false;
		for (auto &tile : tileset.map_part_types)
		{
			if (tile.second->missionObjective)
			{
				hasObjective = true;
				break;
			}
		}
		TEST_REQUIRE(hasObjective, "{0} has no objective map parts", mapName);
	}
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
	    {"ufo_mission_preference_loaded", test_ufo_mission_preference_loaded},
	    {"org_park_funds", test_org_park_funds},
	    {"org_park_sell_surplus", test_org_park_sell_surplus},
	    {"purchase_deduct", test_purchase_deduct},
	    {"cargo_expiry_refund", test_cargo_expiry_refund},
	    {"goto_building_fallback", test_goto_building_fallback},
	    {"destination_gate", test_destination_gate},
	    {"overspawn_invasion", test_overspawn_invasion},
	    {"ufo_mission_counter_decrements_on_arrival",
	     test_ufo_mission_counter_decrements_on_arrival},
	    {"ufo_mission_counter_zero_picks_new_target", test_ufo_mission_counter_zero_picks_new_target},
	    {"ufo_mission_counter_zero_without_building_clears_target",
	     test_ufo_mission_counter_zero_without_building_clears_target},
	    {"ufo_mission_counter_decrements_from_mission_start",
	     test_ufo_mission_counter_decrements_from_mission_start},
	    {"infiltration_display_percent", test_infiltration_display_percent},
	    {"ufo_growth_rates_match_exe", test_ufo_growth_rates_match_exe},
	    {"manufacture_dimension_probe", test_manufacture_dimension_probe},
	    {"research_prereq_all_graphs", test_research_prereq_all_graphs},
	    {"research_prereq_unknown2_any", test_research_prereq_unknown2_any},
	    {"research_prereq_unknown1_any", test_research_prereq_unknown1_any},
	    {"research_aa7a8_hardcoded_gates", test_research_aa7a8_hardcoded_gates},
	    {"alien_building4_keeps_table_prereq", test_alien_building4_keeps_table_prereq},
	    {"alien_building_exe_rows", test_alien_building_exe_rows},
	    {"manufacture_type02_ammo_ids", test_manufacture_type02_ammo_ids},
	    {"manufacture_disruptor_armor_ids", test_manufacture_disruptor_armor_ids},
	    {"craft_ammo_manufacturers", test_craft_ammo_manufacturers},
	    {"craft_ammo_economy_ids", test_craft_ammo_economy_ids},
	    {"vehicle_equipment_ammo_types", test_vehicle_equipment_ammo_types},
	    {"disruptor_multibomb_fragment", test_disruptor_multibomb_fragment},
	    {"dimension_probe_manufacturer", test_dimension_probe_manufacturer},
	    {"research_item_prereq_gates", test_research_item_prereq_gates},
	    {"ufopaedia_alien_craft_group", test_ufopaedia_alien_craft_group},
	    {"detection_weights_from_exe", test_detection_weights_from_exe},
	    {"organic_factory_gates_ufo_growth", test_organic_factory_gates_ufo_growth},
	    {"ufo_incursion_table", test_ufo_incursion_table},
	    {"ufo_incursion_spawn_xy", test_ufo_incursion_spawn_xy},
	    {"ufo_incursion_follow_type", test_ufo_incursion_follow_type},
	    {"subversion_prefers_one_known_base", test_subversion_prefers_one_known_base},
	    {"militarized_from_org_type", test_militarized_from_org_type},
	    {"nearby_intact_buildings", test_nearby_intact_buildings},
	    {"unmanned_ufo_loot", test_unmanned_ufo_loot},
	    {"cequip_score_req", test_cequip_score_req},
	    {"aequip_artifact_and_resist", test_aequip_artifact_and_resist},
	    {"aequip_market_week0", test_aequip_market_week0},
	    {"vequip_economy_hide", test_vequip_economy_hide},
	    {"vehicle_park_spawn", test_vehicle_park_spawn},
	    {"ufopaedia_start_visible", test_ufopaedia_start_visible},
	    {"ufopaedia_economy_hide", test_ufopaedia_economy_hide},
	    {"ufopaedia_abf9c_unlock", test_ufopaedia_abf9c_unlock},
	    {"org_raid_loot_table", test_org_raid_loot_table},
	    {"org_extracted_scalars", test_org_extracted_scalars},
	    {"base_destroy_facility_errors", test_base_destroy_facility_errors},
	    {"alien_building_briefings_extracted", test_alien_building_briefings_extracted},
	    {"alien_building_objectives_present", test_alien_building_objectives_present},
	});
}
