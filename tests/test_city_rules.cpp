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
// U1(a), the other branch. UFO2P FUN_0003a910 tests vehicle +0x12C at the mission-counter-zero
// transition: value 1 latches an "arrived" flag whose reader flies the craft to the nearest
// dimension gate, anything else runs the retarget search the three tests above lock.
//
// For an incursion-spawned UFO that is not a choice. FUN_0006da88 hardcodes the field to 1
// immediately before the write, and no writer anywhere touches it on an already-spawned vehicle,
// so the retarget branch is structurally unreachable for this population -- they always leave.
// docs/original-game/findings/U1-retarget-reconciliation.md settles it, reconciling two earlier
// findings that disagreed.
//
// The retarget branch is still real code in the original, for a different population, which is
// why the tests above stay exactly as they are: this is a split, not a replacement.
// U1(b): a damaged UFO breaks off and leaves through the nearest dimension gate.
//
// UFO2P FUN_000588f8 compares current constitution against a role-derived fraction of the type's
// ceiling and, inside that band, sets the same +0x16A "arrived" flag U1(a) uses -- whose reader
// (FUN_00059148) flies the craft to the nearest gate. See
// docs/original-game/findings/U1b-gate-consumer.md.
//
// The percent is role-indexed and NOT uniform. An earlier pass asserted a flat 75%; that figure
// holds only at a call site this population structurally never reaches, and was corrected by
// raw-verifying FUN_0006da88's only two callers. The table is UFO_WITHDRAW_HEALTH_PERCENT_BY_ROLE.
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

// The ten alien-dimension endgame maps, in InitialGameStateExtractor::battleMapPaths /
// missionObjectives order. TACP 0x2E0C09-0x2E1C98 packs one briefing per map in this order.
static const char *const ALIEN_BUILDING_MAPS[] = {
    "39incub", "40spawn", "41food",   "42megapd", "43sleep",
    "44organ", "45farm",  "46contrl", "47maint",  "48gate",
};

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
	    {"overspawn_invasion", test_overspawn_invasion},
	    {"ufo_mission_counter_decrements_from_mission_start",
	     test_ufo_mission_counter_decrements_from_mission_start},
	    {"manufacture_dimension_probe", test_manufacture_dimension_probe},
	    {"research_prereq_all_graphs", test_research_prereq_all_graphs},
	    {"research_prereq_unknown2_any", test_research_prereq_unknown2_any},
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
	    {"ufo_incursion_follow_type", test_ufo_incursion_follow_type},
	    {"org_raid_loot_table", test_org_raid_loot_table},
	    {"alien_building_objectives_present", test_alien_building_objectives_present},
	});
}
