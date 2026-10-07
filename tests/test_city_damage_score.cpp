#include "framework/configfile.h"
#include "game/state/city/scenery.h"
#include "game/state/gamestate.h"
#include "game/state/shared/organisation.h"
#include "library/sp.h"
#include "tests/test_helpers.h"
#include <iostream>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

// "Damage to City" as UFO2P.EXE scores it (docs/original-game/findings/city-damage-scoring.md):
// a projectile hit on building scenery in the human city is charged when the firer belongs to X-COM
// or the Aliens; other organisations' fire and the alien city are never charged. OpenApoc used to
// charge every destroyed tile, whoever destroyed it, collapses included.
static bool test_who_is_charged()
{
	GameState state;
	for (const auto &id : {"ORG_X-COM", "ORG_ALIEN", "ORG_MEGAPOL"})
	{
		auto org = mksp<Organisation>();
		org->id = id;
		state.organisations[id] = org;
	}
	state.player = {&state, "ORG_X-COM"};
	state.aliens = {&state, "ORG_ALIEN"};
	const StateRef<Organisation> xcom{&state, "ORG_X-COM"}, alien{&state, "ORG_ALIEN"},
	    megapol{&state, "ORG_MEGAPOL"};

	TEST_REQUIRE(Scenery::hitChargesCityDamage(state, "CITYMAP_HUMAN", true, xcom),
	             "X-COM fire on a building is charged");
	TEST_REQUIRE(Scenery::hitChargesCityDamage(state, "CITYMAP_HUMAN", true, alien),
	             "Alien fire on a building is charged to X-COM too");
	TEST_REQUIRE(!Scenery::hitChargesCityDamage(state, "CITYMAP_HUMAN", true, megapol),
	             "another organisation's fire is not charged");
	TEST_REQUIRE(!Scenery::hitChargesCityDamage(state, "CITYMAP_ALIEN", true, xcom),
	             "the alien city is never charged");
	TEST_REQUIRE(!Scenery::hitChargesCityDamage(state, "CITYMAP_HUMAN", false, xcom),
	             "scenery outside a building footprint is not charged");
	TEST_REQUIRE(!Scenery::hitChargesCityDamage(state, "CITYMAP_HUMAN", true, {}),
	             "a hit with no firer org is not charged");

	Scenery::chargeCityDamage(state, 7);
	TEST_REQUIRE(state.weekScore.cityDamage == -7 && state.totalScore.cityDamage == -7,
	             "the charge lands in both the week and the running total");
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	return runTestSuite({{"who_is_charged", test_who_is_charged}});
}
