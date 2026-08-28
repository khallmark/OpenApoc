#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/city/economyinfo.h"
#include "game/state/city/research.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/vequipmenttype.h"
#include "library/sp.h"
#include "library/strings.h"
#include <cstdlib>

// This exercises only the three members added to VEquipmentType on top of vanilla master:
// scoreRequirementFor(), and the isEconomyVisible()/clearEconomyHide()/economyUnhidden trio.
// It does not touch cd.iso or a generated base_gamestate - none of this logic depends on
// extracted data, so a plain in-memory GameState is enough. Nothing in the current codebase
// calls these methods yet (see the PR description for the sibling files that will), so this is
// the only exerciser they have at this base.

using namespace OpenApoc;

static bool testCheckFailed = false;

#define CHECK(cond, ...)                                                                           \
	do                                                                                             \
	{                                                                                              \
		if (!(cond))                                                                               \
		{                                                                                          \
			LogError(__VA_ARGS__);                                                                 \
			testCheckFailed = true;                                                                \
		}                                                                                          \
	} while (0)

static void test_score_requirement_for()
{
	VEquipmentType equip;
	equip.id = "VEQUIPMENTTYPE_TEST_SCORE";
	equip.scoreRequirement = 500;

	// Empty scoreRequirementByDifficulty (the only state an unextended, or non-extractor-fed,
	// VEquipmentType can have) must fall back to the flat scoreRequirement for every difficulty,
	// including out-of-range ones - this is what keeps scoreRequirementFor() byte-identical to
	// master's direct field read when no extractor has populated the vector.
	CHECK(equip.scoreRequirementByDifficulty.empty(),
	      "expected no per-difficulty column by default");
	CHECK(equip.scoreRequirementFor(0) == 500, "novice falls back to flat scoreRequirement");
	CHECK(equip.scoreRequirementFor(4) == 500, "superhuman falls back to flat scoreRequirement");
	CHECK(equip.scoreRequirementFor(-1) == 500, "negative difficulty falls back");
	CHECK(equip.scoreRequirementFor(99) == 500, "out-of-range difficulty falls back");

	// Populated vector (what tools/extractors/extract_vehicle_equipment.cpp would produce from
	// UFO2P's cequip_score_req_data, once that extractor lands - out of scope here) is indexed
	// directly per difficulty, and out-of-range indices still fall back to scoreRequirement
	// rather than reading out of bounds.
	equip.scoreRequirementByDifficulty = {2000, 1750, 1500, 1250, 1000};
	CHECK(equip.scoreRequirementFor(0) == 2000, "novice reads column 0");
	CHECK(equip.scoreRequirementFor(4) == 1000, "superhuman reads column 4");
	CHECK(equip.scoreRequirementFor(2) == 1500, "middle difficulty reads column 2");
	CHECK(equip.scoreRequirementFor(-1) == 500, "negative difficulty still falls back to flat");
	CHECK(equip.scoreRequirementFor(5) == 500, "one-past-the-end still falls back to flat");
}

static void test_economy_visibility_absent_from_economy()
{
	GameState state;
	VEquipmentType equip;
	equip.id = "VEQUIPMENTTYPE_TEST_NO_ECONOMY_ENTRY";
	// Nothing is inserted into state.economy for this id.
	CHECK(!equip.isEconomyVisible(state), "an id with no economy entry must stay hidden");
}

static void test_economy_visibility_week_zero()
{
	GameState state;
	VEquipmentType equip;
	equip.id = "VEQUIPMENTTYPE_TEST_WEEK_ZERO";

	EconomyInfo eco;
	eco.weekAvailable = 0;
	state.economy[equip.id] = eco;

	CHECK(!equip.isEconomyVisible(state),
	      "weekAvailable == 0 hides the item until clearEconomyHide() runs");
	CHECK(!equip.economyUnhidden, "economyUnhidden starts false");

	equip.clearEconomyHide();
	CHECK(equip.economyUnhidden, "clearEconomyHide() sets economyUnhidden");
	CHECK(equip.isEconomyVisible(state),
	      "once unhidden, a weekAvailable == 0 item becomes visible - master's four inlined "
	      "\"weekAvailable == 0 || ...\" checks in transactioncontrol.cpp have no equivalent "
	      "unhide path at all");
}

static void test_economy_visibility_week_gate()
{
	GameState state; // default GameTime -> getWeek() == 1
	CHECK(state.gameTime.getWeek() == 1, "test assumes a fresh GameState starts on week 1");

	VEquipmentType notYet;
	notYet.id = "VEQUIPMENTTYPE_TEST_FUTURE_WEEK";
	EconomyInfo futureEco;
	futureEco.weekAvailable = 999;
	state.economy[notYet.id] = futureEco;
	CHECK(!notYet.isEconomyVisible(state), "weekAvailable in the future stays hidden");

	VEquipmentType available;
	available.id = "VEQUIPMENTTYPE_TEST_CURRENT_WEEK";
	EconomyInfo currentEco;
	currentEco.weekAvailable = 1;
	state.economy[available.id] = currentEco;
	CHECK(available.isEconomyVisible(state),
	      "weekAvailable at or before the current week is visible");
}

static void test_economy_visibility_research_dependency()
{
	GameState state;

	auto topic = mksp<ResearchTopic>();
	topic->man_hours = 100;
	topic->man_hours_progress = 0;
	state.research.topics["RESEARCH_TEST_TOPIC"] = topic;

	VEquipmentType equip;
	equip.id = "VEQUIPMENTTYPE_TEST_RESEARCH_GATED";
	equip.research_dependency.type = ResearchDependency::Type::Any;
	equip.research_dependency.topics.insert(
	    StateRef<ResearchTopic>(&state, UString("RESEARCH_TEST_TOPIC")));

	EconomyInfo eco;
	eco.weekAvailable = 1; // already on sale for the week; only the research gate is at issue
	state.economy[equip.id] = eco;

	CHECK(!equip.isEconomyVisible(state),
	      "an economy-eligible item with an unsatisfied research dependency stays hidden");

	topic->man_hours_progress = topic->man_hours;
	CHECK(topic->isComplete(), "test setup: topic should now read as complete");
	CHECK(equip.isEconomyVisible(state),
	      "completing the dependency makes the item visible without touching economy or "
	      "economyUnhidden at all");
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	test_score_requirement_for();
	test_economy_visibility_absent_from_economy();
	test_economy_visibility_week_zero();
	test_economy_visibility_week_gate();
	test_economy_visibility_research_dependency();

	if (testCheckFailed)
	{
		LogError("test_vequipmenttype FAILED");
		return EXIT_FAILURE;
	}

	LogInfo("test_vequipmenttype success");
	return EXIT_SUCCESS;
}
