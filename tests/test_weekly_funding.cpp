#include "framework/configfile.h"
#include "game/state/city/base.h"
#include "game/state/city/facility.h"
#include "game/state/gamestate.h"
#include "game/state/rules/city/facilitytype.h"
#include "game/state/shared/organisation.h"
#include "library/sp.h"
#include "tests/test_helpers.h"
#include <iostream>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

// Weekly government funding as UFO2P.EXE assesses it (non-4 file 0xF6880; see
// docs/original-game/findings/weekly-funding.md). The old income F is credited first; the score
// tier adjusts F; the result is capped at half the Government's balance and stored as the next
// income; the Government is debited that next income. The permanent cutoff tests the score of the
// weeks *before* the one being assessed, or Government relations below -50.

namespace
{

struct Funding
{
	GameState state;
	sp<Organisation> xcom, gov;

	Funding(int income, int xcomBalance, int govBalance, float relation = 0.0f)
	{
		xcom = mksp<Organisation>();
		xcom->id = "ORG_X-COM";
		gov = mksp<Organisation>();
		gov->id = "ORG_GOVERNMENT";
		state.organisations[xcom->id] = xcom;
		state.organisations[gov->id] = gov;
		state.player = {&state, xcom->id};
		state.government = {&state, gov->id};
		// data/common_patch/gamestate.xml
		state.weekly_rating_rules = {{-1600, -4}, {-800, -5},  {-400, -10}, {0, -15},
		                             {12800, 4},  {6400, 5},   {3200, 8},   {1600, 12},
		                             {800, 16},   {400, 20}};
		xcom->income = income;
		xcom->balance = xcomBalance;
		gov->balance = govBalance;
		gov->establishRelation(state.player, relation);
	}

	// A week that scored `week` on top of `previous` from the weeks before it.
	void scoreWeek(int week, int previous = 0)
	{
		state.weekScore.tacticalMissions = week;
		state.totalScore.tacticalMissions = previous + week;
	}
};

} // namespace

static bool test_credit_cap_and_debit()
{
	// Credit old F (file 0xF6AB2), store min(F + delta, G/2) as income (file 0xF7083), debit
	// Government by that *new* income (file 0xF70B7). Credit and debit differ whenever funding changes.
	Funding rich(92000, 0, 1000000);
	rich.scoreWeek(10000);
	rich.state.weeklyPlayerUpdate();
	TEST_CHECK(rich.xcom->balance == 92000, "X-COM is paid the old income F, got {0}",
	           rich.xcom->balance);
	TEST_CHECK(rich.xcom->income == 110400, "10000 week score: next income F + F/5, got {0}",
	           rich.xcom->income);
	TEST_CHECK(rich.gov->balance == 1000000 - 110400,
	           "the Government pays the new income, not the old one: got {0}", rich.gov->balance);

	Funding poor(92000, 0, 100000);
	poor.scoreWeek(10000);
	poor.state.weeklyPlayerUpdate();
	TEST_CHECK(poor.xcom->balance == 92000,
	           "the cap limits next week's income, not this week's payment: got {0}",
	           poor.xcom->balance);
	TEST_CHECK(poor.xcom->income == 50000, "next income capped at half the Government's 100000, "
	                                       "got {0}",
	           poor.xcom->income);
	TEST_CHECK(poor.gov->balance == 50000, "the Government is debited the capped next income, "
	                                       "got {0}",
	           poor.gov->balance);
	const auto &a = poor.state.fundingAssessment;
	TEST_CHECK(a.outcome == FundingAssessment::Outcome::Assessed && a.oldIncome == 92000 &&
	               a.scoreAdjustment == 18400 && a.capAdjustment == 50000 - 110400 &&
	               a.nextIncome == 50000 && a.week.getTotal() == 10000,
	           "the report record keeps old {0}, score {1}, cap {2}, next {3}, week {4}",
	           a.oldIncome, a.scoreAdjustment, a.capAdjustment, a.nextIncome, a.week.getTotal());

	Funding broke(92000, 0, -10);
	broke.scoreWeek(10000);
	broke.state.weeklyPlayerUpdate();
	TEST_CHECK(broke.xcom->balance == 92000 && broke.xcom->income == 0 && broke.gov->balance == -10,
	           "a Government with no money sets the next income to 0 (file 0xF7025): balance {0} "
	           "income {1} gov {2}",
	           broke.xcom->balance, broke.xcom->income, broke.gov->balance);
	TEST_CHECK(!broke.state.fundingTerminated, "a cap to zero is not the termination latch");
	return true;
}

static bool test_tiers_divide_the_old_income()
{
	// Strict thresholds, strongest matching tier wins, every tier divides the unchanged F.
	const std::vector<std::pair<int, int>> cases = {
	    {12801, 92000 + 23000}, {12800, 92000 + 18400}, {401, 92000 + 4600}, {400, 92000},
	    {0, 92000},             {-1, 92000 - 6133},     {-401, 92000 - 9200}, {-1601, 92000 - 23000},
	};
	for (const auto &[week, expected] : cases)
	{
		Funding f(92000, 0, 10000000);
		f.scoreWeek(week);
		f.state.weeklyPlayerUpdate();
		TEST_CHECK(f.xcom->income == expected, "week score {0}: next income {1}, got {2}", week,
		           expected, f.xcom->income);
	}
	return true;
}

static bool test_cutoff_uses_the_previous_weeks()
{
	// File 0xF6FBF: CMP ECX,-2400 where ECX is the previous weeks' score H, not H + W.
	Funding f(92000, 0, 10000000);
	f.scoreWeek(-3000, 0);
	f.state.weeklyPlayerUpdate();
	TEST_CHECK(!f.state.fundingTerminated,
	           "a first bad week (H=0, W=-3000) cuts the income, it does not end funding");
	TEST_CHECK(f.xcom->income == 69000, "W=-3000 takes the -25% tier: got {0}", f.xcom->income);
	TEST_CHECK(f.state.weekScore.getTotal() == 0,
	           "the simulation rolls the week's score over, not the report screen: week={0}",
	           f.state.weekScore.getTotal());

	// The following week recovers (+1000), so the lifetime total is -2000 -- but H is now -3000.
	f.state.weekScore = {};
	f.state.totalScore.tacticalMissions = -3000;
	f.scoreWeek(1000, -3000);
	const int balanceBefore = f.xcom->balance;
	const int govBefore = f.gov->balance;
	f.state.weeklyPlayerUpdate();
	TEST_CHECK(f.state.fundingTerminated,
	           "H=-3000 < -2400 cuts funding even though the week recovered the total to -2000");
	TEST_CHECK(f.xcom->income == 0, "cut: income 0, got {0}", f.xcom->income);
	TEST_CHECK(f.xcom->balance == balanceBefore,
	           "cut: the week's credit is taken back (file 0xF7090): {0} -> {1}", balanceBefore,
	           f.xcom->balance);
	TEST_CHECK(f.gov->balance == govBefore, "cut: the Government is not debited: {0} -> {1}",
	           govBefore, f.gov->balance);
	TEST_CHECK(f.state.fundingAssessment.outcome == FundingAssessment::Outcome::CutForScore &&
	               f.state.fundingAssessment.previousWeeksScore == -3000,
	           "the report shows the score cutoff the week it happens");

	f.scoreWeek(500, -2000);
	f.state.weeklyPlayerUpdate();
	TEST_CHECK(f.state.fundingAssessment.outcome == FundingAssessment::Outcome::None &&
	               f.state.weekScore.getTotal() == 0,
	           "after the cutoff there is nothing to assess, but the week still rolls over");
	return true;
}

static bool test_alien_buildings_destroyed_counts()
{
	GameScore score;
	score.cityDamage = -100;
	score.alienBuildingsDestroyed = 500;
	TEST_CHECK(score.getTotal() == 400, "the eighth category counts in the total: {0}",
	           score.getTotal());
	score.reset();
	TEST_CHECK(score.alienBuildingsDestroyed == 0 && score.getTotal() == 0,
	           "and resets with the rest");
	return true;
}

static bool test_cutoff_boundaries()
{
	{
		Funding f(92000, 0, 10000000);
		f.scoreWeek(0, -2400);
		f.state.weeklyPlayerUpdate();
		TEST_CHECK(!f.state.fundingTerminated, "H=-2400 is not below -2400 (JGE)");
	}
	{
		Funding f(92000, 0, 10000000);
		f.scoreWeek(0, -2401);
		f.state.weeklyPlayerUpdate();
		TEST_CHECK(f.state.fundingTerminated, "H=-2401 cuts funding");
	}
	{
		Funding f(92000, 0, 10000000, -50.0f);
		f.state.weeklyPlayerUpdate();
		TEST_CHECK(!f.state.fundingTerminated, "Government relation -50 is not below -50");
	}
	{
		Funding f(92000, 0, 10000000, -51.0f);
		f.state.weeklyPlayerUpdate();
		TEST_CHECK(f.state.fundingTerminated, "Government relation -51 cuts funding");
	}
	{
		// The latch is permanent: a later good week and friendly relations do not restore it.
		Funding f(92000, 0, 10000000, -51.0f);
		f.state.weeklyPlayerUpdate();
		f.gov->establishRelation(f.state.player, 80.0f);
		f.scoreWeek(20000);
		const int balance = f.xcom->balance;
		f.state.weeklyPlayerUpdate();
		TEST_CHECK(f.state.fundingTerminated && f.xcom->income == 0 && f.xcom->balance == balance,
		           "funding stays cut: income {0}, balance {1} -> {2}", f.xcom->income, balance,
		           f.xcom->balance);
	}
	return true;
}

static bool test_two_billion_credit_guard()
{
	// File 0xF6AA8: CMP EAX,0x77359400; JGE skips the credit when the balance is already >= 2e9.
	Funding f(92000, 2000000000, 1000000);
	f.state.weeklyPlayerUpdate();
	TEST_CHECK(f.xcom->balance == 2000000000, "no credit at 2,000,000,000: got {0}",
	           f.xcom->balance);
	TEST_CHECK(f.xcom->income == 92000 && f.gov->balance == 1000000 - 92000,
	           "the assessment and the Government debit still happen: income {0}, gov {1}",
	           f.xcom->income, f.gov->balance);
	return true;
}

static bool test_unfinished_facilities_cost_nothing()
{
	// File 0xFB211: maintenance skips facilities whose construction byte is non-zero.
	Funding f(0, 100000, 0);
	auto type = mksp<FacilityType>();
	type->weeklyCost = 1000;
	f.state.facility_types["FACILITYTYPE_TEST"] = type;
	auto built = mksp<Facility>(StateRef<FacilityType>{&f.state, "FACILITYTYPE_TEST"});
	auto building = mksp<Facility>(StateRef<FacilityType>{&f.state, "FACILITYTYPE_TEST"});
	building->buildTime = 5;
	auto base = mksp<Base>();
	base->facilities = {built, building};
	f.state.player_bases["BASE_TEST"] = base;
	f.state.weeklyPlayerUpdate();
	TEST_CHECK(f.xcom->balance == 100000 - 1000,
	           "only the finished facility pays upkeep: balance {0}", f.xcom->balance);
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	return runTestSuite({{"credit_cap_and_debit", test_credit_cap_and_debit},
	                     {"tiers_divide_the_old_income", test_tiers_divide_the_old_income},
	                     {"cutoff_uses_the_previous_weeks", test_cutoff_uses_the_previous_weeks},
	                     {"cutoff_boundaries", test_cutoff_boundaries},
	                     {"two_billion_credit_guard", test_two_billion_credit_guard},
	                     {"unfinished_facilities_cost_nothing",
	                      test_unfinished_facilities_cost_nothing},
	                     {"alien_buildings_destroyed_counts", test_alien_buildings_destroyed_counts}});
}
