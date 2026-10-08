// Read-only player knowledge: statistics follow rules, while research and ownership gate access.
#include "framework/configfile.h"
#include "game/state/city/base.h"
#include "game/state/city/research.h"
#include "game/state/gamestate.h"
#include "game/state/gamestateintrospect.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/rules/battle/damage.h"
#include "game/state/shared/organisation.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_catalog_visibility_and_live_statistics()
{
	GameState state;
	state.organisations["ORG_X-COM"] = mksp<Organisation>();
	state.player = {&state, "ORG_X-COM"};
	state.player_bases["BASE_TEST"] = mksp<Base>();
	state.current_base = {&state, "BASE_TEST"};
	auto equipment = mksp<AEquipmentType>();
	equipment->id = "AEQUIPMENTTYPE_FLIGHT_TEST";
	equipment->name = "Flight test";
	equipment->type = AEquipmentType::Type::Armor;
	equipment->equipscreen_size = {3, 3};
	equipment->armor = 37;
	equipment->weight = 12;
	equipment->provides_flight = true;
	equipment->store_space = 50;
	state.agent_equipment[equipment->id] = equipment;
	auto &market = state.economy[equipment->id];
	market.currentPrice = 1234;
	market.currentStock = 7;
	const auto inventories = state.current_base->inventoryAgentEquipment;
	const auto before = introspectGameState(state, "equipment_catalog");
	TEST_REQUIRE(before.find("name=Flight_test:kind=Armor:slot=Body:armor=37:weight=12:flight=1") !=
	                 UString::npos,
	             "flight and armor come from rules: {0}", before);
	TEST_REQUIRE(before.find("price=1234:market=7:store_space=50") != UString::npos,
	             "market statistics: {0}", before);
	TEST_REQUIRE(state.current_base->inventoryAgentEquipment == inventories &&
	                 market.currentStock == 7,
	             "query must not insert into stores or buy stock");
	equipment->armor = 83;
	TEST_REQUIRE(introspectGameState(state, "equipment_catalog").find("armor=83") != UString::npos,
	             "modded armor value must be read live");

	// A future market row must not reveal unknown equipment. Owned researched kit stays visible.
	market.weekAvailable = 100;
	TEST_REQUIRE(introspectGameState(state, "equipment_catalog").find(equipment->id) ==
	                 UString::npos,
	             "future unowned row hidden");
	state.current_base->inventoryAgentEquipment[equipment->id] = 2;
	TEST_REQUIRE(introspectGameState(state, "equipment_catalog").find("BASE_TEST~2") !=
	                 UString::npos,
	             "owned equipment visible even without a market row");
	auto topic = mksp<ResearchTopic>();
	topic->name = equipment->name;
	topic->man_hours = 10;
	state.research.topics["RESEARCH_TEST"] = topic;
	equipment->research_dependency.topics.emplace(&state, UString("RESEARCH_TEST"));
	TEST_REQUIRE(introspectGameState(state, "equipment_catalog").find(equipment->id) ==
	                 UString::npos,
	             "unresearched equipment stats hidden even if owned");
	topic->man_hours_progress = 10;
	TEST_REQUIRE(introspectGameState(state, "equipment_catalog").find(equipment->id) !=
	                 UString::npos,
	             "completed research unlocks owned stats");
	return true;
}

static bool test_damage_modifiers_do_not_reveal_unknown_species()
{
	GameState state;
	state.organisations["ORG_X-COM"] = mksp<Organisation>();
	state.player = {&state, "ORG_X-COM"};
	state.damage_modifiers["DAMAGEMODIFIER_TERRAIN_TEST"] = mksp<DamageModifier>();
	state.damage_modifiers["DAMAGEMODIFIER_ALIEN_TEST"] = mksp<DamageModifier>();
	auto damage = mksp<DamageType>();
	damage->modifiers[{&state, UString("DAMAGEMODIFIER_TERRAIN_TEST")}] = 25;
	damage->modifiers[{&state, UString("DAMAGEMODIFIER_ALIEN_TEST")}] = 80;
	state.damage_types["DAMAGETYPE_TEST"] = damage;
	auto gun = mksp<AEquipmentType>();
	gun->id = "AEQUIPMENTTYPE_TEST_GUN";
	gun->type = AEquipmentType::Type::Weapon;
	gun->equipscreen_size = {2, 4};
	gun->damage = 65;
	gun->accuracy = 40;
	gun->fire_delay = 32;
	gun->damage_type = {&state, "DAMAGETYPE_TEST"};
	state.agent_equipment[gun->id] = gun;
	state.economy[gun->id].currentStock = 1;
	auto alien = mksp<AgentType>();
	alien->name = "Test alien";
	alien->damage_modifier = {&state, "DAMAGEMODIFIER_ALIEN_TEST"};
	state.agent_types["AGENTTYPE_TEST_ALIEN"] = alien;
	auto topic = mksp<ResearchTopic>();
	topic->name = alien->name;
	topic->man_hours = 10;
	state.research.topics["RESEARCH_TEST_ALIEN"] = topic;
	const auto unknown = introspectGameState(state, "equipment_catalog");
	TEST_REQUIRE(unknown.find("DAMAGEMODIFIER_TERRAIN_TEST~25") != UString::npos,
	             "terrain modifier supplied");
	TEST_REQUIRE(unknown.find("DAMAGEMODIFIER_ALIEN_TEST~80") == UString::npos,
	             "unresearched species modifier hidden");
	topic->man_hours_progress = 10;
	TEST_REQUIRE(
	    introspectGameState(state, "equipment_catalog").find("DAMAGEMODIFIER_ALIEN_TEST~80") !=
	        UString::npos,
	    "researched species modifier supplied");
	topic->hidden = true;
	TEST_REQUIRE(
	    introspectGameState(state, "equipment_catalog").find("DAMAGEMODIFIER_ALIEN_TEST~80") ==
	        UString::npos,
	    "hidden unlock topics cannot expose species");
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
		return EXIT_FAILURE;
	applyDeterministicTestConfig();
	return runTestSuite(
	    {{"catalog visibility and stats", test_catalog_visibility_and_live_statistics},
	     {"known damage modifiers", test_damage_modifiers_do_not_reveal_unknown_species}});
}
