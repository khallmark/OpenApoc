#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/city/agentmission.h"
#include "game/state/city/base.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/research.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/agenttype.h"
#include "game/state/rules/city/ufogrowth.h"
#include "game/state/rules/city/ufoincursion.h"
#include "game/state/rules/city/ufomissionpreference.h"
#include "game/state/rules/city/vammotype.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/rules/city/vequipmenttype.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/doodad.h"
#include "game/state/shared/organisation.h"
#include "library/rect.h"
#include "library/sp.h"
#include "tests/test_helpers.h"
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

static bool test_advanced_quantum_lab_any()
{
	auto &state = *g_state;
	auto it = state.research.topics.find("RESEARCH_ADVANCED_QUANTUM_PHYSICS_LAB");
	TEST_REQUIRE(it != state.research.topics.end() && it->second,
	             "RESEARCH_ADVANCED_QUANTUM_PHYSICS_LAB missing");
	const auto &deps = it->second->dependencies.research;
	TEST_REQUIRE(deps.size() == 1, "quantum lab groups {0} (extractor All ANDs patch Any)",
	             deps.size());
	TEST_REQUIRE(deps.front().type == ResearchDependency::Type::Any, "quantum lab must be Any");
	TEST_REQUIRE(deps.front().topics.size() == 3, "quantum lab topic count {0}",
	             deps.front().topics.size());
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

static bool test_craft_ammo_manufacturers()
{
	auto &state = *g_state;
	// UFO2P non-4 craft_ammo_manufacturers_data at 0x13EB6A: uint16[15] org index.
	// Zorium is org 0 (X-COM), not Solmine.
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
	return true;
}

static bool test_ufopaedia_alien_craft_group()
{
	auto &state = *g_state;
	TEST_REQUIRE(state.ufopaedia.find("PAEDIACATEGORY_ALIEN_CRAFT") != state.ufopaedia.end(),
	             "PAEDIACATEGORY_ALIEN_CRAFT missing");
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
	    {"infiltration_display_percent", test_infiltration_display_percent},
	    {"ufo_growth_rates_match_exe", test_ufo_growth_rates_match_exe},
	    {"manufacture_dimension_probe", test_manufacture_dimension_probe},
	    {"advanced_quantum_lab_any", test_advanced_quantum_lab_any},
	    {"alien_building4_keeps_table_prereq", test_alien_building4_keeps_table_prereq},
	    {"manufacture_type02_ammo_ids", test_manufacture_type02_ammo_ids},
	    {"craft_ammo_manufacturers", test_craft_ammo_manufacturers},
	    {"research_item_prereq_gates", test_research_item_prereq_gates},
	    {"ufopaedia_alien_craft_group", test_ufopaedia_alien_craft_group},
	    {"organic_factory_gates_ufo_growth", test_organic_factory_gates_ufo_growth},
	    {"ufo_incursion_table", test_ufo_incursion_table},
	    {"militarized_from_org_type", test_militarized_from_org_type},
	    {"nearby_intact_buildings", test_nearby_intact_buildings},
	    {"unmanned_ufo_loot", test_unmanned_ufo_loot},
	});
}
