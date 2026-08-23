#include "framework/configfile.h"
#include "game/state/gamestate.h"
#include "game/state/shared/organisation.h"
#include "library/sp.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool addOrg(GameState &state, const UString &id, int balance, sp<Organisation> &out)
{
	out = mksp<Organisation>();
	out->id = id;
	out->balance = balance;
	state.organisations[id] = out;
	return true;
}

static bool test_bribe_cost()
{
	GameState state;
	state.difficulty = 0;
	sp<Organisation> target;
	sp<Organisation> other;
	addOrg(state, "ORG_TARGET", 100000, target);
	addOrg(state, "ORG_OTHER", 1000000, other);
	StateRef<Organisation> otherRef{&state, "ORG_OTHER"};

	target->current_relations[otherRef] = 75.0f;
	TEST_REQUIRE(target->costOfBribeBy(state, otherRef) == 0, "allied bribe cost should be 0");

	target->current_relations[otherRef] = -51.0f;
	const int hostileCost = target->costOfBribeBy(state, otherRef);
	target->current_relations[otherRef] = 0.0f;
	const int neutralCost = target->costOfBribeBy(state, otherRef);
	TEST_REQUIRE(hostileCost > 0, "hostile bribe cost {0}", hostileCost);
	TEST_REQUIRE(neutralCost > hostileCost, "neutral cost {0} should exceed hostile {1}",
	             neutralCost, hostileCost);
	return true;
}

static bool test_bribed_by()
{
	GameState state;
	state.difficulty = 0;
	sp<Organisation> target;
	sp<Organisation> other;
	addOrg(state, "ORG_TARGET", 0, target);
	addOrg(state, "ORG_OTHER", 1000000, other);
	StateRef<Organisation> otherRef{&state, "ORG_OTHER"};

	target->current_relations[otherRef] = 0.0f;
	const int cost = target->costOfBribeBy(state, otherRef);
	TEST_REQUIRE(cost > 0, "expected positive bribe cost");
	TEST_REQUIRE(!target->bribedBy(state, otherRef, 0), "zero bribe should fail");
	TEST_REQUIRE(!target->bribedBy(state, otherRef, cost - 1), "underpay should fail");
	TEST_REQUIRE(target->bribedBy(state, otherRef, cost), "exact cost should succeed");
	TEST_REQUIRE(target->getRelationTo(otherRef) == 25.0f, "neutral bribe should reach 25, got {0}",
	             target->getRelationTo(otherRef));
	TEST_REQUIRE(other->balance == 1000000 - cost, "payer balance {0}", other->balance);
	TEST_REQUIRE(target->balance == cost, "target balance {0}", target->balance);
	return true;
}

static bool test_sign_treaty()
{
	GameState state;
	sp<Organisation> target;
	sp<Organisation> other;
	addOrg(state, "ORG_TARGET", 0, target);
	addOrg(state, "ORG_OTHER", 50000, other);
	StateRef<Organisation> otherRef{&state, "ORG_OTHER"};

	target->current_relations[otherRef] = -20.0f;
	target->signTreatyWith(state, otherRef, 1000, false);
	TEST_REQUIRE(target->getRelationTo(otherRef) == 0.0f,
	             "negative relation treaty should go to 0, got {0}",
	             target->getRelationTo(otherRef));
	TEST_REQUIRE(target->balance == 1000, "treaty bribe not applied");
	TEST_REQUIRE(other->balance == 49000, "payer not charged");

	target->signTreatyWith(state, otherRef, 0, true);
	TEST_REQUIRE(target->getRelationTo(otherRef) == 100.0f, "forceAlliance should be 100, got {0}",
	             target->getRelationTo(otherRef));
	return true;
}

static bool test_player_relation_mirror()
{
	GameState state;
	sp<Organisation> player;
	sp<Organisation> megapol;
	addOrg(state, "ORG_X-COM", 0, player);
	addOrg(state, "ORG_MEGAPOL", 0, megapol);
	state.player = {&state, "ORG_X-COM"};

	StateRef<Organisation> playerRef{&state, "ORG_X-COM"};
	megapol->adjustRelationTo(state, playerRef, -10.0f);
	TEST_REQUIRE(megapol->getRelationTo(playerRef) == -10.0f, "org side {0}",
	             megapol->getRelationTo(playerRef));
	TEST_REQUIRE(player->getRelationTo({&state, "ORG_MEGAPOL"}) == -10.0f,
	             "player mirror {0}", player->getRelationTo({&state, "ORG_MEGAPOL"}));

	// Player-initiated adjust does not clobber the org's view (raid deltas stay asymmetric).
	player->adjustRelationTo(state, {&state, "ORG_MEGAPOL"}, 7.0f);
	TEST_REQUIRE(player->getRelationTo({&state, "ORG_MEGAPOL"}) == -3.0f, "player after {0}",
	             player->getRelationTo({&state, "ORG_MEGAPOL"}));
	TEST_REQUIRE(megapol->getRelationTo(playerRef) == -10.0f, "org view must stay -10, got {0}",
	             megapol->getRelationTo(playerRef));
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
	    {"bribe_cost", test_bribe_cost},
	    {"bribed_by", test_bribed_by},
	    {"sign_treaty", test_sign_treaty},
	    {"player_relation_mirror", test_player_relation_mirror},
	});
}
