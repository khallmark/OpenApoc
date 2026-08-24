#include "forms/harness_ui.h"
#include "forms/ui.h"
#include "framework/configfile.h"
#include "framework/framework.h"
#include "game/ui/boot.h"
#include "game/ui/components/controlgenerator.h"
#include "game/ui/general/transactioncontrol.h"
#include "game/ui/tileview/cityview.h"
#include "version.h"
#include <SDL_main.h>

using namespace OpenApoc;

int main(int argc, char *argv[])
{
	if (config().parseOptions(argc, argv))
	{
		return EXIT_FAILURE;
	}
	LogInfo("Starting OpenApoc \"{0}\"", OPENAPOC_VERSION);

	{
		up<Framework> fw(new Framework(UString(argv[0]), true));

		// Let the harness see the live control tree rather than guessing layout from XML.
		registerFormsHarnessUI();

		fw->run(mksp<BootUp>());

		// Release renderer-backed resources while the renderer is still alive. ControlGenerator
		// keeps its icon cache in a static singleton, so otherwise those images are destroyed at
		// process exit -- after the renderer -- leaving their GL textures undeleted.
		ControlGenerator::releaseCachedImages();
		TransactionControl::releaseCachedImages();
		UI::unload();
	}

	return 0;
}
