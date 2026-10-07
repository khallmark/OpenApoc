#include "framework/configfile.h"
#include "game/state/gameevent.h"
#include "game/state/gameeventtypes.h"
#include "tests/test_helpers.h"
#include <iostream>
#include <stdexcept>

using namespace OpenApoc;

// NotificationScreen::resume() looked its event up with GameEvent::optionsMap.at(). resume() runs
// when a modal stacked above the notification pops, inside Framework::run's stage-command drain,
// and CityView pushes an AlienTakeover notification unconditionally -- an event optionsMap has no
// entry for. Two takeovers in one event batch, or any modal over a takeover, therefore aborted the
// game with an uncaught map::at. The decision now lives in GameEvent::pausesFor().
int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}

	// Control: the expression the old resume() evaluated really does throw for this event. If
	// AlienTakeover ever gains an option this stops holding, and the test below stops proving
	// anything -- so say so rather than pass quietly.
	bool oldLookupThrew = false;
	try
	{
		(void)GameEvent::optionsMap.at(GameEventType::AlienTakeover);
	}
	catch (const std::out_of_range &)
	{
		oldLookupThrew = true;
	}
	TEST_REQUIRE(oldLookupThrew,
	             "AlienTakeover has no notification option, so the old optionsMap.at() throws");

	TEST_REQUIRE(GameEvent::pausesFor(GameEventType::AlienTakeover),
	             "an event with no option always pauses, and does not throw");

	// An event with an option follows it both ways.
	config().set("Notifications.City.UfoSpotted", true);
	TEST_REQUIRE(GameEvent::pausesFor(GameEventType::UfoSpotted), "option on: pause");
	config().set("Notifications.City.UfoSpotted", false);
	TEST_REQUIRE(!GameEvent::pausesFor(GameEventType::UfoSpotted), "option off: no pause");

	std::cout << "notification pause decision handles events with and without an option\n";
	return EXIT_SUCCESS;
}
