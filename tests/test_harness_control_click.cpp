#include "forms/control.h"
#include "forms/form.h"
#include "forms/graphicbutton.h"
#include "forms/harness_actions.h"
#include "forms/textbutton.h"
#include "forms/ui.h"
#include "framework/configfile.h"
#include "framework/event.h"
#include "framework/framework.h"
#include "framework/harness.h"
#include "tests/test_helpers.h"
#include <SDL_mouse.h>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

static bool test_control_click()
{
	auto form = mksp<Form>();
	form->Location = {100, 80};
	form->Size = {200, 100};
	auto control = form->createChild<Control>();
	control->Name = "CLICK_TARGET";
	control->Location = {30, 20};
	control->Size = {40, 30};
	form->update();

	int clicks = 0;
	int leftClicks = 0;
	FrameworkMouseEvent observed = {};
	sp<Control> raisedBy;
	control->addCallback(FormEventType::MouseClick,
	                     [&](FormsEvent *e)
	                     {
		                     clicks++;
		                     observed = e->forms().MouseInfo;
		                     raisedBy = e->forms().RaisedBy;
		                     if (observed.Button == SDL_BUTTON_LMASK &&
							     Event::isPressed(observed.Button, Event::MouseButton::Left))
		                     {
			                     leftClicks++;
		                     }
	                     });

	HarnessCommand command;
	TEST_REQUIRE(parseHarnessCommand("CONTROL CLICK_TARGET click", command), "parse CONTROL click");
	auto handler = getHarnessActionHandler();
	TEST_REQUIRE(handler, "forms harness action handler installed");
	TEST_REQUIRE(handler(command.text, command.args) == "OK clicked CLICK_TARGET", "click reply");
	TEST_REQUIRE(clicks == 1, "CONTROL click must invoke the callback once, got {0}", clicks);
	TEST_REQUIRE(leftClicks == 1,
	             "CONTROL click callback received button={0}, expected left mask={1}",
	             observed.Button, SDL_BUTTON_LMASK);
	TEST_REQUIRE(raisedBy == control, "CONTROL click must identify the target control");
	TEST_REQUIRE(observed.X == 20 && observed.Y == 15,
	             "CONTROL click must use local centre (20, 15), got ({0}, {1})", observed.X,
	             observed.Y);
	TEST_REQUIRE(observed.DeltaX == 0 && observed.DeltaY == 0 && observed.WheelVertical == 0 &&
	                 observed.WheelHorizontal == 0 && !observed.TouchStartedAsPan,
	             "CONTROL click must have no motion, scrolling or touch pan");

	const auto synthetic = observed;
	const auto position = control->getLocationOnScreen() + Vec2<int>{20, 15};
	for (auto type : {EVENT_MOUSE_DOWN, EVENT_MOUSE_UP})
	{
		MouseEvent event(type);
		event.mouse() = {};
		event.mouse().X = position.x;
		event.mouse().Y = position.y;
		event.mouse().Button = SDL_BUTTON_LMASK;
		form->eventOccured(&event);
	}
	TEST_REQUIRE(clicks == 2 && leftClicks == 2, "real left click must invoke the same callback");
	TEST_REQUIRE(observed.X == synthetic.X && observed.Y == synthetic.Y &&
	                 observed.Button == synthetic.Button && observed.DeltaX == synthetic.DeltaX &&
	                 observed.DeltaY == synthetic.DeltaY &&
	                 observed.WheelVertical == synthetic.WheelVertical &&
	                 observed.WheelHorizontal == synthetic.WheelHorizontal &&
	                 observed.TouchStartedAsPan == synthetic.TouchStartedAsPan,
	             "CONTROL click mouse fields must match the real left click");
	return true;
}

static bool test_first_frame_buttons()
{
	for (bool animations : {false, true})
	{
		config().set("OpenApoc.NewFeature.UiAnimations", animations);
		auto form = mksp<Form>();
		form->Size = {200, 100};
		auto graphic = form->createChild<GraphicButton>();
		auto text = form->createChild<TextButton>();
		graphic->Name = "FIRST_FRAME_GRAPHIC";
		text->Name = "FIRST_FRAME_TEXT";
		graphic->Size = text->Size = {40, 30};
		text->Location = {50, 0};
		form->update();

		// The harness must not need a rendered frame or an animation update before clicking.
		for (sp<Control> button :
		     {std::static_pointer_cast<Control>(graphic), std::static_pointer_cast<Control>(text)})
		{
			int clicks = 0;
			button->addCallback(FormEventType::MouseClick, [&](FormsEvent *) { clicks++; });
			const auto position = button->getLocationOnScreen();
			HarnessCommand command;
			TEST_REQUIRE(parseHarnessCommand("CONTROL " + button->Name + " click", command),
			             "parse first-frame button click");
			TEST_REQUIRE(getHarnessActionHandler()(command.text, command.args) ==
			                 "OK clicked " + button->Name,
			             "button must be available immediately with animations={0}", animations);
			TEST_REQUIRE(clicks == 1 && button->Enabled && button->isVisible(),
			             "first-frame click must fire once without changing availability");
			TEST_REQUIRE(button->getLocationOnScreen() == position &&
			                 button->Size == Vec2<int>(40, 30),
			             "feedback must preserve hit rectangles");
		}
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
	Framework fw("OpenApoc", false);
	const int result = runTestSuite({{"CONTROL left click", test_control_click},
	                                 {"first-frame animated buttons", test_first_frame_buttons}});
	UI::unload();
	return result;
}
