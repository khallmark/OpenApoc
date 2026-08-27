#include "framework/configfile.h"
#include "framework/data.h"
#include "framework/framework.h"
#include "game/state/city/agentmission.h"
#include "game/state/city/base.h"
#include "game/state/city/building.h"
#include "game/state/city/city.h"
#include "game/state/city/facility.h"
#include "game/state/city/research.h"
#include "game/state/city/vehicle.h"
#include "game/state/city/vehiclemission.h"
#include "game/state/gamestate.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/agenttype.h"
#include "game/state/rules/battle/battlemap.h"
#include "game/state/rules/battle/battlemapparttype.h"
#include "game/state/rules/battle/battlemaptileset.h"
#include "game/state/rules/city/facilitytype.h"
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

static bool test_ufopaedia_alien_craft_group()
{
	auto &state = *g_state;
	TEST_REQUIRE(state.ufopaedia.find("PAEDIACATEGORY_ALIEN_CRAFT") != state.ufopaedia.end(),
	             "PAEDIACATEGORY_ALIEN_CRAFT missing");
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
	    {"goto_building_fallback", test_goto_building_fallback},
	    {"manufacture_dimension_probe", test_manufacture_dimension_probe},
	    {"research_prereq_all_graphs", test_research_prereq_all_graphs},
	    {"alien_building4_keeps_table_prereq", test_alien_building4_keeps_table_prereq},
	    {"manufacture_type02_ammo_ids", test_manufacture_type02_ammo_ids},
	    {"manufacture_disruptor_armor_ids", test_manufacture_disruptor_armor_ids},
	    {"craft_ammo_economy_ids", test_craft_ammo_economy_ids},
	    {"vehicle_equipment_ammo_types", test_vehicle_equipment_ammo_types},
	    {"ufopaedia_alien_craft_group", test_ufopaedia_alien_craft_group},
	    {"org_raid_loot_table", test_org_raid_loot_table},
	    {"alien_building_objectives_present", test_alien_building_objectives_present},
	});
}
