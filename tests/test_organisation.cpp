#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/state/city/building.h"
#include "game/state/city/vehicle.h"
#include "game/state/gamestate.h"
#include "game/state/rules/agenttype.h"
#include "game/state/shared/organisation.h"
#include "library/sp.h"
#include "library/xorshift.h"
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

static bool test_raid_relation_pressure()
{
	GameState state;
	sp<Organisation> a;
	sp<Organisation> b;
	addOrg(state, "ORG_A", a);
	addOrg(state, "ORG_B", b);
	StateRef<Organisation> other{&state, "ORG_B"};
	StateRef<Organisation> self{&state, "ORG_A"};

	a->current_relations[other] = 0.0f;
	a->long_term_relations[other] = 20.0f;
	TEST_REQUIRE(a->raidRelationPressure(other) == 20.0f, "pressure before snapshot is {0}",
	             a->raidRelationPressure(other));
	a->updateRelations(self);
	TEST_REQUIRE(a->raidRelationPressure(other) == 1.0f, "pressure after snapshot is {0}",
	             a->raidRelationPressure(other));
	return true;
}

static bool test_micronoid_rain_takeover()
{
	Framework fw("OpenApoc", false);
	GameState state;
	sp<Organisation> player;
	sp<Organisation> aliens;
	sp<Organisation> victim;
	addOrg(state, "ORG_XCOM", player);
	addOrg(state, "ORG_ALIEN", aliens);
	addOrg(state, "ORG_VICTIM", victim);
	state.player = {&state, "ORG_XCOM"};
	state.aliens = {&state, "ORG_ALIEN"};

	TEST_REQUIRE(!victim->tryMicronoidRain(state, 0), "chance 0 must fail");
	TEST_REQUIRE(!victim->takenOver, "chance 0 must not take over");

	TEST_REQUIRE(victim->tryMicronoidRain(state, 100), "chance 100 must succeed");
	TEST_REQUIRE(victim->takenOver, "subversion must set takenOver");
	TEST_REQUIRE(victim->infiltrationValue == 200, "infiltration {0}", victim->infiltrationValue);
	TEST_REQUIRE(victim->militarized, "taken-over org militarizes");
	StateRef<Organisation> playerRef{&state, "ORG_XCOM"};
	TEST_REQUIRE(victim->getRelationTo(playerRef) == -100.0f, "hostile to player {0}",
	             victim->getRelationTo(playerRef));
	TEST_REQUIRE(!victim->tryMicronoidRain(state, 100), "already taken over");
	return true;
}

static bool test_taken_over_infiltration_clamp()
{
	// FUN_0007fcc0 @ VA 0x7FCC0: taken-over org infiltration is forced to 200.
	GameState state;
	sp<Organisation> player;
	sp<Organisation> aliens;
	sp<Organisation> victim;
	addOrg(state, "ORG_XCOM", player);
	addOrg(state, "ORG_ALIEN", aliens);
	addOrg(state, "ORG_VICTIM", victim);
	state.player = {&state, "ORG_XCOM"};
	state.aliens = {&state, "ORG_ALIEN"};

	victim->takenOver = true;
	victim->infiltrationValue = 40;
	victim->updateInfiltration(state);
	TEST_REQUIRE(victim->infiltrationValue == 200, "taken-over clamp {0}",
	             victim->infiltrationValue);
	return true;
}

static bool test_infiltration_hourly_ufo2p_rules()
{
	// FUN_0007fcc0 is hourly: civilian (index 27) skipped; X-COM is in the
	// 0..0x1A loop; odd hour decrements the stored value. Empty buildings
	// stay unchanged regardless of the 42 − difficulty divisor.
	GameState state;
	sp<Organisation> player;
	sp<Organisation> aliens;
	sp<Organisation> civilian;
	sp<Organisation> victim;
	addOrg(state, "ORG_XCOM", player);
	addOrg(state, "ORG_ALIEN", aliens);
	addOrg(state, "ORG_CIVILIAN", civilian);
	addOrg(state, "ORG_VICTIM", victim);
	state.player = {&state, "ORG_XCOM"};
	state.aliens = {&state, "ORG_ALIEN"};
	state.civilian = {&state, "ORG_CIVILIAN"};

	state.difficulty = 4;
	state.gameTime = GameTime(0); // 00:00, even hour
	victim->infiltrationValue = 10;
	player->infiltrationValue = 10;
	civilian->infiltrationValue = 10;
	victim->updateInfiltration(state);
	player->updateInfiltration(state);
	civilian->updateInfiltration(state);
	TEST_REQUIRE(victim->infiltrationValue == 10, "difficulty must not change even-hour value {0}",
	             victim->infiltrationValue);
	TEST_REQUIRE(player->infiltrationValue == 10, "even-hour X-COM stays {0}",
	             player->infiltrationValue);
	TEST_REQUIRE(civilian->infiltrationValue == 10, "civilian skipped even hour {0}",
	             civilian->infiltrationValue);

	state.gameTime = GameTime(TICKS_PER_HOUR); // 01:00, odd hour
	victim->updateInfiltration(state);
	player->updateInfiltration(state);
	civilian->updateInfiltration(state);
	TEST_REQUIRE(victim->infiltrationValue == 9, "odd hour decrements victim {0}",
	             victim->infiltrationValue);
	TEST_REQUIRE(player->infiltrationValue == 9, "odd hour decrements X-COM {0}",
	             player->infiltrationValue);
	TEST_REQUIRE(civilian->infiltrationValue == 10, "civilian skipped odd hour {0}",
	             civilian->infiltrationValue);
	return true;
}

static bool test_infiltration_divisor_uses_difficulty()
{
	// FUN_0007fcc0 @ file 0xD2364 (ISO non-4 CRC 0x4749ffc1):
	// divisor = 42 − ([0x10C9A] >> 16). Writers at file 0xC4DA8 store
	// difficulty 0..4 at obj2+0x10C9C. 4-build file 0xD21EC is
	// `42 - word[0x10C9C]`. Product 1×42×38×1 = 1596 → /42 = 38, /38 = 42.
	GameState state;
	sp<Organisation> player;
	sp<Organisation> aliens;
	sp<Organisation> victim;
	addOrg(state, "ORG_XCOM", player);
	addOrg(state, "ORG_ALIEN", aliens);
	addOrg(state, "ORG_VICTIM", victim);
	state.player = {&state, "ORG_XCOM"};
	state.aliens = {&state, "ORG_ALIEN"};

	auto fn = mksp<BuildingFunction>();
	fn->infiltrationSpeed = 38;
	state.building_functions["BUILDINGFUNCTION_TEST"] = fn;

	auto alien = mksp<AgentType>();
	alien->infiltrationSpeed = 42;
	state.agent_types["AGENTTYPE_TEST"] = alien;

	auto building = mksp<Building>();
	building->function = {&state, "BUILDINGFUNCTION_TEST"};
	building->current_crew[{&state, "AGENTTYPE_TEST"}] = 1;
	state.buildings["BUILDING_TEST"] = building;
	victim->buildings.push_back({&state, "BUILDING_TEST"});
	victim->infiltrationSpeed = 1;

	state.gameTime = GameTime(0);
	state.difficulty = 0;
	victim->infiltrationValue = 10;
	victim->updateInfiltration(state);
	TEST_REQUIRE(victim->infiltrationValue == 48, "novice increment {0}",
	             victim->infiltrationValue);

	state.difficulty = 4;
	victim->infiltrationValue = 10;
	victim->updateInfiltration(state);
	TEST_REQUIRE(victim->infiltrationValue == 52, "superhuman increment {0}",
	             victim->infiltrationValue);
	return true;
}

static bool test_get_guard_count()
{
	// FUN_000aec70 @ file 0x101314: FUN_0005d1d8(50); (avg * (roll+75)) / 100; cap 20.
	TEST_REQUIRE(Organisation::guardCountFromRoll(16, 0) == 12, "roll 0 → 12");
	TEST_REQUIRE(Organisation::guardCountFromRoll(16, 25) == 16, "roll 25 → 16");
	TEST_REQUIRE(Organisation::guardCountFromRoll(16, 50) == 20, "roll 50 → 20");
	TEST_REQUIRE(Organisation::guardCountFromRoll(40, 0) == 20, "40*75/100=30 caps at 20");
	// Old ±25% band would draw [12,20] uniformly; EXE is one 0..50 roll.
	GameState state;
	sp<Organisation> org;
	addOrg(state, "ORG_A", org);
	org->average_guards = 16;
	state.rng.seed(1);
	auto probe = state.rng;
	const int roll = randBoundsInclusive(probe, 0, 50);
	TEST_REQUIRE(org->getGuardCount(state) == Organisation::guardCountFromRoll(16, roll),
	             "live path must use FUN_0005d1d8(50)");
	org->average_guards = 40;
	for (int i = 0; i < 16; i++)
	{
		const int n = org->getGuardCount(state);
		TEST_REQUIRE(n == 20, "cap 20 produced {0}", n);
	}
	return true;
}

static bool test_raid_manpower()
{
	// FUN_00092060 @ file 0xE4704: (qty/100)*avg*avg; att<def Low; def*2<att High unless Megapol.
	TEST_REQUIRE(Organisation::raidManpower(500, 16) == 1280, "500/100*16*16 → 1280");
	TEST_REQUIRE(Organisation::raidManpower(50, 16) == 0, "50/100 truncates to 0");
	TEST_REQUIRE(Organisation::raidManpower(5000, 16) == 12800, "raid fallback 5000/100*256");
	TEST_REQUIRE(Organisation::raidManpowerBucket(100, 200, false) ==
	                 Organisation::RaidManpowerBucket::Low,
	             "att < def is Low");
	TEST_REQUIRE(Organisation::raidManpowerBucket(200, 100, false) ==
	                 Organisation::RaidManpowerBucket::Normal,
	             "2*def == att stays Normal");
	TEST_REQUIRE(Organisation::raidManpowerBucket(201, 100, false) ==
	                 Organisation::RaidManpowerBucket::High,
	             "2*def < att is High");
	TEST_REQUIRE(Organisation::raidManpowerBucket(201, 100, true) ==
	                 Organisation::RaidManpowerBucket::Normal,
	             "Megapol attacker cannot take High");
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
	    {"raid_relation_pressure", test_raid_relation_pressure},
	    {"micronoid_rain_takeover", test_micronoid_rain_takeover},
	    {"taken_over_infiltration_clamp", test_taken_over_infiltration_clamp},
	    {"infiltration_hourly_ufo2p_rules", test_infiltration_hourly_ufo2p_rules},
	    {"infiltration_divisor_uses_difficulty", test_infiltration_divisor_uses_difficulty},
	    {"get_guard_count", test_get_guard_count},
	    {"raid_manpower", test_raid_manpower},
	});
}
