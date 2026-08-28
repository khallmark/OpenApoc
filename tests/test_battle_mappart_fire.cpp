#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/battle/battlemappart.h"

using namespace OpenApoc;

// BattleMapPart::fireStageBurns() is a pure function of two ints, so this test needs no
// GameState/tileobject/gamestate-data plumbing - it just pins the threshold comparison that
// decides whether a fire overlay stage is old enough to ignite the terrain it sits on.
namespace
{
struct Case
{
	int fireStage;
	int fireBurnTime;
	bool expected;
	const char *description;
};
} // namespace

static bool test_fire_stage_burns_case(const Case &c)
{
	bool actual = BattleMapPart::fireStageBurns(c.fireStage, c.fireBurnTime);
	if (actual != c.expected)
	{
		LogError("fireStageBurns(fireStage={0}, fireBurnTime={1}) [{2}]: expected {3}, got {4}",
		         c.fireStage, c.fireBurnTime, c.description, c.expected ? "true" : "false",
		         actual ? "true" : "false");
		return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	// clang-format off
	const Case cases[] = {
	    // fireStage, fireBurnTime, expected, description
	    {-1, 10,  false, "negative fireStage guards a non-fire overlay"},
	    {10, 255, false, "fireBurnTime == 255 is the incombustible sentinel"},
	    {0,  0,   true,  "degenerate but live: stage 0 already meets a 0 threshold"},
	    {5,  5,   true,  "equal stage/threshold burns - the comparison is >=, not >"},
	    {4,  5,   false, "one stage short of the threshold does not burn"},
	    {10, -1,  false, "negative fireBurnTime is rejected"},
	};
	// clang-format on

	bool ok = true;
	for (const auto &c : cases)
	{
		ok &= test_fire_stage_burns_case(c);
	}

	if (!ok)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
