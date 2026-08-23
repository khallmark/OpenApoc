#include "framework/configfile.h"
#include "game/state/city/research.h"
#include "game/state/gamestate.h"
#include "library/sp.h"
#include "tests/test_helpers.h"
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
	    {"exe_slide_crcs", test_exe_slide_crcs},
	});
}
