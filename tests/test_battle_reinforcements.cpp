#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/battle/battle.h"
#include "library/vec.h"
#include "library/xorshift.h"
#include <exception>
#include <set>
#include <vector>

using namespace OpenApoc;

// Battle::spawnReinforcements draws up to 4 reinforcement tiles through
// Battle::spawnAtRandomLocations. A map can offer fewer free pad tiles than that, and the draw
// must then stop rather than call pickRandom on an empty set (which throws).

static std::set<Vec3<int>> makeTiles(int count)
{
	std::set<Vec3<int>> tiles;
	for (int i = 0; i < count; i++)
	{
		tiles.insert(Vec3<int>{i, 2 * i, 1});
	}
	return tiles;
}

// Every pool smaller than the request yields each tile exactly once, and nothing throws.
static bool test_small_pools_yield_every_tile()
{
	LogInfo("Testing pools smaller than the request...");
	for (int size = 0; size <= 3; size++)
	{
		Xorshift128Plus<uint32_t> rng{static_cast<uint64_t>(size + 1)};
		auto tiles = makeTiles(size);
		std::vector<Vec3<int>> drawn;
		try
		{
			Battle::spawnAtRandomLocations(rng, tiles, 4,
			                               [&](Vec3<int> pos)
			                               {
				                               drawn.push_back(pos);
				                               return true;
			                               });
		}
		catch (const std::exception &e)
		{
			LogError("Pool of {0} tiles threw: {1}", size, e.what());
			return false;
		}
		if ((int)drawn.size() != size)
		{
			LogError("Pool of {0} tiles drew {1}, expected {0}", size, drawn.size());
			return false;
		}
		std::set<Vec3<int>> distinct(drawn.begin(), drawn.end());
		if (distinct != tiles)
		{
			LogError("Pool of {0} tiles did not draw each tile exactly once", size);
			return false;
		}
	}
	LogInfo("Small pool test passed");
	return true;
}

// With enough tiles the draw sequence, interleaved with the caller's own RNG use (the unit type
// pick), is the one the previous fixed four-iteration loop produced for the same seed.
static bool test_large_pool_keeps_draw_order()
{
	LogInfo("Testing that a large pool keeps the previous draw order...");
	const std::vector<int> types = {0, 1, 2};
	for (uint64_t seed = 1; seed <= 32; seed++)
	{
		std::vector<std::pair<Vec3<int>, int>> expected;
		{
			Xorshift128Plus<uint32_t> rng{seed};
			auto tiles = makeTiles(6);
			for (int i = 0; i < 4; i++)
			{
				auto pos = pickRandom(rng, tiles);
				tiles.erase(pos);
				expected.push_back({pos, pickRandom(rng, types)});
			}
		}

		std::vector<std::pair<Vec3<int>, int>> actual;
		Xorshift128Plus<uint32_t> rng{seed};
		Battle::spawnAtRandomLocations(rng, makeTiles(6), 4,
		                               [&](Vec3<int> pos)
		                               {
			                               actual.push_back({pos, pickRandom(rng, types)});
			                               return true;
		                               });
		if (actual != expected)
		{
			LogError("Seed {0}: draw sequence differs from the previous loop", seed);
			return false;
		}
	}
	LogInfo("Draw order test passed");
	return true;
}

// spawnAt returning false (the side's unit cap) stops the draw at once.
static bool test_callback_stops_the_draw()
{
	LogInfo("Testing that the spawn callback can stop the draw...");
	Xorshift128Plus<uint32_t> rng{7};
	int calls = 0;
	Battle::spawnAtRandomLocations(rng, makeTiles(6), 4, [&](Vec3<int>) { return ++calls < 2; });
	if (calls != 2)
	{
		LogError("Callback asked to stop after 2 spawns, was called {0} times", calls);
		return false;
	}
	LogInfo("Callback stop test passed");
	return true;
}

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	if (!test_small_pools_yield_every_tile())
	{
		return EXIT_FAILURE;
	}
	if (!test_large_pool_keeps_draw_order())
	{
		return EXIT_FAILURE;
	}
	if (!test_callback_stops_the_draw())
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
