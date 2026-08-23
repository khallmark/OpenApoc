#include "framework/configfile.h"
#include "game/state/city/building.h"
#include "game/state/city/vehicle.h"
#include "game/state/gamestate.h"
#include "game/state/shared/organisation.h"
#include "library/sp.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool addOrg(GameState &state, const UString &id, sp<Organisation> &out)
{
	out = mksp<Organisation>();
	out->id = id;
	state.organisations[id] = out;
	return true;
}

static bool test_self_relation()
{
	GameState state;
	sp<Organisation> org;
	addOrg(state, "ORG_A", org);
	StateRef<Organisation> self{&state, "ORG_A"};
	TEST_REQUIRE(org->getRelationTo(self) == 100.0f, "self relation is {0}",
	             org->getRelationTo(self));
	return true;
}

static bool test_relation_bands()
{
	GameState state;
	sp<Organisation> a;
	sp<Organisation> b;
	addOrg(state, "ORG_A", a);
	addOrg(state, "ORG_B", b);
	StateRef<Organisation> other{&state, "ORG_B"};

	struct Case
	{
		float value;
		Organisation::Relation relation;
	};
	const Case cases[] = {
	    {-51.0f, Organisation::Relation::Hostile},    {-50.0f, Organisation::Relation::Unfriendly},
	    {-26.0f, Organisation::Relation::Unfriendly}, {-25.0f, Organisation::Relation::Neutral},
	    {24.0f, Organisation::Relation::Neutral},     {25.0f, Organisation::Relation::Friendly},
	    {74.0f, Organisation::Relation::Friendly},    {75.0f, Organisation::Relation::Allied},
	};
	for (const auto &c : cases)
	{
		a->current_relations[other] = c.value;
		TEST_REQUIRE(a->isRelatedTo(other) == c.relation, "value {0} mapped to {1}", c.value,
		             (int)a->isRelatedTo(other));
	}
	return true;
}

static bool test_adjust_clamp_and_sign()
{
	GameState state;
	sp<Organisation> a;
	sp<Organisation> b;
	addOrg(state, "ORG_A", a);
	addOrg(state, "ORG_B", b);
	StateRef<Organisation> other{&state, "ORG_B"};

	a->adjustRelationTo(state, other, 150.0f);
	TEST_REQUIRE(a->getRelationTo(other) == 100.0f, "clamp high {0}", a->getRelationTo(other));
	a->adjustRelationTo(state, other, -250.0f);
	TEST_REQUIRE(a->getRelationTo(other) == -100.0f, "clamp low {0}", a->getRelationTo(other));

	a->current_relations[other] = 10.0f;
	TEST_REQUIRE(a->isPositiveTo(other), "10 should be positive");
	TEST_REQUIRE(!a->isNegativeTo(other), "10 should not be negative");
	a->current_relations[other] = 0.0f;
	TEST_REQUIRE(a->isPositiveTo(other), "0 should be positive");
	TEST_REQUIRE(!a->isNegativeTo(other), "0 should not be negative");
	a->current_relations[other] = -1.0f;
	TEST_REQUIRE(!a->isPositiveTo(other), "-1 should not be positive");
	TEST_REQUIRE(a->isNegativeTo(other), "-1 should be negative");
	return true;
}

static bool test_can_purchase_hostile()
{
	GameState state;
	sp<Organisation> seller;
	sp<Organisation> buyerOrg;
	addOrg(state, "ORG_SELLER", seller);
	addOrg(state, "ORG_BUYER", buyerOrg);

	auto building = mksp<Building>();
	building->owner = {&state, "ORG_BUYER"};
	state.buildings["BUILDING_1"] = building;
	StateRef<Building> buyer{&state, "BUILDING_1"};

	seller->current_relations[{&state, "ORG_BUYER"}] = -51.0f;
	TEST_REQUIRE(seller->canPurchaseFrom(state, buyer, true) ==
	                 Organisation::PurchaseResult::OrgHostile,
	             "hostile seller should return OrgHostile");
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
	    {"self_relation", test_self_relation},
	    {"relation_bands", test_relation_bands},
	    {"adjust_clamp_and_sign", test_adjust_clamp_and_sign},
	    {"can_purchase_hostile", test_can_purchase_hostile},
	});
}
