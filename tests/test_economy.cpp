#include "framework/configfile.h"
#include "game/state/city/economyinfo.h"
#include "game/state/gamestate.h"
#include "game/state/gametime.h"
#include "tests/test_helpers.h"

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static EconomyInfo sampleEconomy()
{
	EconomyInfo e;
	e.weekAvailable = 1;
	e.basePrice = 100;
	e.minStock = 10;
	e.maxStock = 40;
	e.currentPrice = 100;
	e.currentStock = 100;
	e.lastStock = 20;
	return e;
}

static bool test_future_week_untouched()
{
	GameState state;
	state.gameTime = GameTime(0);
	TEST_REQUIRE(state.gameTime.getWeek() == 1, "week at t=0 is {0}", state.gameTime.getWeek());
	auto e = sampleEconomy();
	e.weekAvailable = 10;
	const int stock = e.currentStock;
	const int price = e.currentPrice;
	const int last = e.lastStock;
	TEST_REQUIRE(e.update(state, true) == false, "future weekAvailable should return false");
	TEST_REQUIRE(e.currentStock == stock, "stock mutated");
	TEST_REQUIRE(e.currentPrice == price, "price mutated");
	TEST_REQUIRE(e.lastStock == last, "lastStock mutated");
	return true;
}

static bool test_week_one_returns_false()
{
	GameState state;
	state.gameTime = GameTime(0);
	state.rng.seed(1);
	auto e = sampleEconomy();
	e.weekAvailable = 1;
	TEST_REQUIRE(e.update(state, true) == false, "week 1 should return false even if weekAvailable==1");
	return true;
}

static bool test_xcom_stock_bands()
{
	GameState state;
	state.gameTime = GameTime(7 * TICKS_PER_DAY);
	TEST_REQUIRE(state.gameTime.getWeek() >= 2, "expected week >= 2, got {0}",
	             state.gameTime.getWeek());

	bool saw80 = false;
	bool saw66 = false;
	bool sawUnchanged = false;
	for (uint64_t seed = 1; seed <= 64; seed++)
	{
		state.rng.seed(static_cast<uint32_t>(seed));
		auto e = sampleEconomy();
		e.weekAvailable = 1;
		e.currentStock = 100;
		e.lastStock = 100;
		e.maxStock = 10;
		e.update(state, true);
		TEST_REQUIRE(e.lastStock == 100, "xcom lastStock should become previous currentStock");
		TEST_REQUIRE(e.currentPrice >= e.basePrice / 2 && e.currentPrice <= e.basePrice,
		             "xcom price {0} outside [{1},{2}]", e.currentPrice, e.basePrice / 2,
		             e.basePrice);
		if (e.currentStock == 80)
		{
			saw80 = true;
		}
		else if (e.currentStock == 66)
		{
			saw66 = true;
		}
		else if (e.currentStock == 100)
		{
			sawUnchanged = true;
		}
		else
		{
			TEST_REQUIRE(false, "xcom stock {0} is not an 80%/66%/unchanged band", e.currentStock);
		}
	}
	TEST_REQUIRE(saw80 && saw66 && sawUnchanged, "did not observe all xcom stock bands");
	return true;
}

static bool test_non_xcom_bands()
{
	GameState state;
	state.gameTime = GameTime(7 * TICKS_PER_DAY);
	for (uint64_t seed = 1; seed <= 16; seed++)
	{
		state.rng.seed(static_cast<uint32_t>(seed));
		auto e = sampleEconomy();
		e.weekAvailable = 2;
		e.currentStock = 20;
		e.lastStock = 20;
		e.minStock = 10;
		e.maxStock = 40;
		e.currentPrice = 100;
		e.basePrice = 100;
		TEST_CHECK(e.update(state, false) == true, "weekAvailable==week>1 should return true");
		TEST_REQUIRE(e.currentStock >= e.minStock && e.currentStock <= e.maxStock,
		             "non-xcom stock {0} outside [{1},{2}]", e.currentStock, e.minStock,
		             e.maxStock);
		TEST_REQUIRE(e.currentPrice >= e.basePrice / 2 && e.currentPrice <= e.basePrice * 2,
		             "non-xcom price {0} outside clamp", e.currentPrice);
	}
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
	    {"future_week_untouched", test_future_week_untouched},
	    {"week_one_returns_false", test_week_one_returns_false},
	    {"xcom_stock_bands", test_xcom_stock_bands},
	    {"non_xcom_bands", test_non_xcom_bands},
	});
}
