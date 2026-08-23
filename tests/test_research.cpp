#include "framework/configfile.h"
#include "game/state/city/base.h"
#include "game/state/city/research.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/vequipmenttype.h"
#include "library/sp.h"
#include "tests/test_helpers.h"
#include "tools/extractors/common/agent.h"
#include "tools/extractors/common/building.h"
#include "tools/extractors/common/exe_slide.h"
#include "tools/extractors/common/research.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static sp<ResearchTopic> addTopic(GameState &state, const UString &id, ResearchTopic::Type type,
                                  unsigned hours, unsigned progress)
{
	auto topic = mksp<ResearchTopic>();
	topic->type = type;
	topic->man_hours = hours;
	topic->man_hours_progress = progress;
	state.research.topics[id] = topic;
	return topic;
}

static bool test_is_complete()
{
	ResearchTopic biochem;
	biochem.type = ResearchTopic::Type::BioChem;
	biochem.man_hours = 10;
	biochem.man_hours_progress = 0;
	TEST_REQUIRE(!biochem.isComplete(), "0/10 should be incomplete");
	biochem.man_hours_progress = 9;
	TEST_REQUIRE(!biochem.isComplete(), "9/10 should be incomplete");
	biochem.man_hours_progress = 10;
	TEST_REQUIRE(biochem.isComplete(), "10/10 biochem should be complete");
	biochem.man_hours_progress = 11;
	TEST_REQUIRE(biochem.isComplete(), "11/10 biochem should be complete");

	ResearchTopic physics;
	physics.type = ResearchTopic::Type::Physics;
	physics.man_hours = 5;
	physics.man_hours_progress = 5;
	TEST_REQUIRE(physics.isComplete(), "physics at hours should be complete");

	ResearchTopic engineering;
	engineering.type = ResearchTopic::Type::Engineering;
	engineering.man_hours = 5;
	engineering.man_hours_progress = 5;
	TEST_REQUIRE(!engineering.isComplete(), "engineering is never complete via isComplete");
	return true;
}

static bool test_force_complete()
{
	ResearchTopic topic;
	topic.man_hours = 20;
	topic.man_hours_progress = 3;
	topic.started = false;
	topic.forceComplete();
	TEST_REQUIRE(topic.man_hours_progress == 20, "forceComplete progress {0}",
	             topic.man_hours_progress);
	TEST_REQUIRE(topic.started, "forceComplete should set started");
	return true;
}

static bool test_research_dependency()
{
	GameState state;
	addTopic(state, "RESEARCH_A", ResearchTopic::Type::BioChem, 10, 0);
	addTopic(state, "RESEARCH_B", ResearchTopic::Type::BioChem, 10, 10);

	ResearchDependency empty;
	TEST_REQUIRE(empty.satisfied(), "empty dependency is always satisfied");

	ResearchDependency any;
	any.type = ResearchDependency::Type::Any;
	any.topics.emplace(&state, UString("RESEARCH_A"));
	any.topics.emplace(&state, UString("RESEARCH_B"));
	TEST_REQUIRE(any.satisfied(), "Any should be true when B is complete");

	ResearchDependency anyIncomplete;
	anyIncomplete.type = ResearchDependency::Type::Any;
	anyIncomplete.topics.emplace(&state, UString("RESEARCH_A"));
	TEST_REQUIRE(!anyIncomplete.satisfied(), "Any should be false when A is incomplete");

	ResearchDependency all;
	all.type = ResearchDependency::Type::All;
	all.topics.emplace(&state, UString("RESEARCH_A"));
	all.topics.emplace(&state, UString("RESEARCH_B"));
	TEST_REQUIRE(!all.satisfied(), "All should be false when A is incomplete");

	state.research.topics["RESEARCH_A"]->man_hours_progress = 10;
	TEST_REQUIRE(all.satisfied(), "All should be true when both complete");
	return true;
}

static bool test_item_dependency_any()
{
	GameState state;
	auto light = mksp<VEquipmentType>();
	light->id = "VEQUIPMENTTYPE_LIGHT";
	state.vehicle_equipment["VEQUIPMENTTYPE_LIGHT"] = light;
	auto heavy = mksp<VEquipmentType>();
	heavy->id = "VEQUIPMENTTYPE_HEAVY";
	state.vehicle_equipment["VEQUIPMENTTYPE_HEAVY"] = heavy;
	auto base = mksp<Base>();
	state.player_bases["BASE_TEST"] = base;
	StateRef<Base> baseRef{&state, "BASE_TEST"};
	base->inventoryVehicleEquipment["VEQUIPMENTTYPE_HEAVY"] = 1;

	ItemDependency any;
	any.type = ItemDependency::Type::Any;
	any.vehicleItemsRequired[{&state, UString("VEQUIPMENTTYPE_LIGHT")}] = 1;
	any.vehicleItemsRequired[{&state, UString("VEQUIPMENTTYPE_HEAVY")}] = 1;
	TEST_REQUIRE(any.satisfied(baseRef), "Any should be true when Heavy is held");

	ItemDependency all;
	all.type = ItemDependency::Type::All;
	all.vehicleItemsRequired[{&state, UString("VEQUIPMENTTYPE_LIGHT")}] = 1;
	all.vehicleItemsRequired[{&state, UString("VEQUIPMENTTYPE_HEAVY")}] = 1;
	TEST_REQUIRE(!all.satisfied(baseRef), "All should be false when Light is missing");
	return true;
}

static bool test_exe_slide_crcs()
{
	int32_t slide = -1;
	TEST_REQUIRE(ufo2pFileSlide(UFO2P_CRC_NON4, slide) && slide == 0, "ufo2p non-4");
	TEST_REQUIRE(ufo2pFileSlide(UFO2P_CRC_4, slide) && slide == 0xE00, "ufo2p 4-build");
	TEST_REQUIRE(!ufo2pFileSlide(0xdeadbeef, slide) && slide == 0, "ufo2p unknown");
	TEST_REQUIRE(tacpFileSlide(TACP_CRC_NON4, slide) && slide == 0, "tacp non-4");
	TEST_REQUIRE(tacpFileSlide(TACP_CRC_4, slide) && slide == -0x2200, "tacp 4-build");
	TEST_REQUIRE(!tacpFileSlide(0xdeadbeef, slide) && slide == 0, "tacp unknown");
	TEST_REQUIRE(ufo2pTableOffset(0xE00, CREW_UFO_DOWNED_OFFSET_START) ==
	                 CREW_UFO_DOWNED_OFFSET_START,
	             "crew_ufo_downed does not slide");
	TEST_REQUIRE(ufo2pTableOffset(0xE00, RESEARCH_DATA_OFFSET_START) ==
	                 RESEARCH_DATA_OFFSET_START + 0xE00,
	             "research_data slides +0xE00");
	return true;
}

static bool test_alien_lifeform_prereq_ids()
{
	// UFO2P non-4 research_data type 3: 15 live slots, dead = live+15.
	TEST_REQUIRE(ufo2pAlienLifeformItemId(0) == "AEQUIPMENTTYPE_MULTIWORM_EGG_ALIVE", "slot 0");
	TEST_REQUIRE(ufo2pAlienLifeformItemId(1) == "AEQUIPMENTTYPE_BRAINSUCKER_ALIVE", "slot 1");
	TEST_REQUIRE(ufo2pAlienLifeformItemId(12) == "AEQUIPMENTTYPE_MICRONOID_AGGREGATE_ALIVE",
	             "slot 12");
	TEST_REQUIRE(ufo2pAlienLifeformItemId(13) == "AEQUIPMENTTYPE_BRAINSUCKER_POD", "slot 13");
	TEST_REQUIRE(ufo2pAlienLifeformItemId(14) == "AEQUIPMENTTYPE_OVERSPAWN_ALIVE", "slot 14");
	TEST_REQUIRE(ufo2pAlienLifeformItemId(15) == "AEQUIPMENTTYPE_MULTIWORM_EGG_DEAD", "dead 0");
	TEST_REQUIRE(ufo2pAlienLifeformItemId(16) == "AEQUIPMENTTYPE_BRAINSUCKER_DEAD", "dead 1");
	TEST_REQUIRE(ufo2pAlienLifeformItemId(27) == "AEQUIPMENTTYPE_MICRONOID_AGGREGATE_DEAD",
	             "dead 12");
	TEST_REQUIRE(ufo2pAlienLifeformItemId(28) == "", "pod has no dead");
	TEST_REQUIRE(ufo2pAlienLifeformItemId(29) == "AEQUIPMENTTYPE_OVERSPAWN_DEAD", "dead 14");
	TEST_REQUIRE(ufo2pAlienLifeformItemId(30) == "", "oob");
	return true;
}

static bool test_detection_weight_table_bounds()
{
	// UFO2P non-4 file 0x155354 / 0x142374. 4-build +0xE00.
	TEST_REQUIRE(BUILDING_DETECTION_WEIGHT_COUNT == 49, "49 building-function weights");
	TEST_REQUIRE(BUILDING_DETECTION_WEIGHT_OFFSET_END - BUILDING_DETECTION_WEIGHT_OFFSET_START ==
	                 BUILDING_DETECTION_WEIGHT_COUNT * 4,
	             "building weight span");
	TEST_REQUIRE(ALIEN_INFILTRATION_SLOT_COUNT == 13, "13 alien infiltration slots");
	TEST_REQUIRE(ALIEN_DETECTION_WEIGHT_OFFSET_END - ALIEN_DETECTION_WEIGHT_OFFSET_START ==
	                 ALIEN_INFILTRATION_SLOT_COUNT * 4,
	             "alien weight span");
	TEST_REQUIRE(ALIEN_MOVEMENT_PERCENT_OFFSET_START - ALIEN_DETECTION_WEIGHT_OFFSET_END == 8,
	             "8-byte pad between alien det and move");
	TEST_REQUIRE(ALIEN_MOVEMENT_PERCENT_OFFSET_END == 0x1423E4, "move table ends at 0x1423E4");
	return true;
}

static bool test_aa7a8_hardcoded_topic_lists()
{
	// Listing @ VA 0xAAABE / file 0x10D162. 0xDE420 = 0xDE2B8 + 36*10.
	TEST_REQUIRE((sizeof(UFO2P_AA7A8_LIFE_CYCLE_ALL) / sizeof(UFO2P_AA7A8_LIFE_CYCLE_ALL[0])) == 7,
	             "life cycle All is 12..17 plus topic 36");
	TEST_REQUIRE(UString(UFO2P_AA7A8_LIFE_CYCLE_ALL[0]) == "RESEARCH_MULTIWORM_AUTOPSY",
	             "life cycle starts at topic 12");
	TEST_REQUIRE(UString(UFO2P_AA7A8_LIFE_CYCLE_ALL[6]) == "RESEARCH_THE_ALIEN_GENETIC_STRUCTURE",
	             "CMP [0xDE420] is Genetic Structure");
	TEST_REQUIRE((sizeof(UFO2P_AA7A8_THREAT_ALL) / sizeof(UFO2P_AA7A8_THREAT_ALL[0])) == 24,
	             "threat All is 8..31");
	TEST_REQUIRE((sizeof(UFO2P_AA7A8_BIOCHEM_ANY) / sizeof(UFO2P_AA7A8_BIOCHEM_ANY[0])) == 27,
	             "biochem Any is 7..33");
	TEST_REQUIRE(UString(UFO2P_AA7A8_BIOCHEM_ANY[0]) == "RESEARCH_BRAINSUCKER_PODS",
	             "biochem starts at topic 7");
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
	    {"is_complete", test_is_complete},
	    {"force_complete", test_force_complete},
	    {"research_dependency", test_research_dependency},
	    {"item_dependency_any", test_item_dependency_any},
	    {"exe_slide_crcs", test_exe_slide_crcs},
	    {"alien_lifeform_prereq_ids", test_alien_lifeform_prereq_ids},
	    {"detection_weight_table_bounds", test_detection_weight_table_bounds},
	    {"aa7a8_hardcoded_topic_lists", test_aa7a8_hardcoded_topic_lists},
	});
}
