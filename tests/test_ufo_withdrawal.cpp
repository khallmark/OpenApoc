#include "framework/configfile.h"
#include "framework/logger.h"
#include "game/state/city/vehicle.h"

using namespace OpenApoc;

namespace
{

// OPE-11: recovered damaged-UFO withdrawal band, UFO2P FUN_000588f8. This locks
// Vehicle::withdrawBandEntered() as a pure decision, independent of any city/battle state, so the
// boundary math can be pinned down without standing up a Vehicle, GameState, or City.
bool checkBand(int health, int maxHealth, int crashHealth, int percent, bool expected,
               const char *label)
{
	bool actual = Vehicle::withdrawBandEntered(health, maxHealth, crashHealth, percent);
	if (actual != expected)
	{
		LogError("withdrawBandEntered(health={0}, maxHealth={1}, crashHealth={2}, percent={3}) "
		         "[{4}] returned {5}, expected {6}",
		         health, maxHealth, crashHealth, percent, label, actual, expected);
		return false;
	}
	return true;
}

} // anonymous namespace

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	bool ok = true;

	// percent == 0 is "never withdraws", regardless of current health.
	ok &= checkBand(50, 100, 20, 0, false, "zero percent disables withdrawal");
	ok &= checkBand(20, 100, 20, 0, false, "zero percent disables withdrawal at crash health");

	// Normal band: maxHealth=100, crashHealth=20, percent=50 -> threshold=50, band is [20, 50).
	ok &= checkBand(20, 100, 20, 50, true, "inclusive lower bound at crash health");
	ok &= checkBand(35, 100, 20, 50, true, "mid band");
	ok &= checkBand(49, 100, 20, 50, true, "just below threshold");
	ok &= checkBand(50, 100, 20, 50, false, "exclusive upper bound at threshold");
	ok &= checkBand(19, 100, 20, 50, false, "one below crash health");
	ok &= checkBand(100, 100, 20, 50, false, "full health, above band");

	// Low-percent roles (Escort / role 9 at 10%): threshold(10) <= crashHealth(20), so the band
	// is empty by construction -- no health value can ever satisfy it. This falls out of the
	// comparison rather than needing a special case for those roles.
	ok &= checkBand(15, 100, 20, 10, false, "low-percent role: between threshold and crash health");
	ok &=
	    checkBand(20, 100, 20, 10, false, "low-percent role: at crash health, still >= threshold");
	ok &= checkBand(5, 100, 20, 10, false, "low-percent role: below crash health");

	// Degenerate hull ceiling guards against dividing by (or against) a zero/negative max health.
	ok &= checkBand(0, 0, 0, 50, false, "zero max health");
	ok &= checkBand(-5, 100, 20, 50, false, "negative current health");

	if (!ok)
	{
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
