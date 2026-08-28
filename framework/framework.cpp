#include "framework/framework.h"
#include "framework/ThreadPool/ThreadPool.h"
#include "framework/apocresources/cursor.h"
#include "framework/configfile.h"
#include "framework/data.h"
#include "framework/event.h"
#include "framework/filesystem.h"
#include "framework/harness.h"
#include "framework/image.h"
#include "framework/jukebox.h"
#include "framework/logger_file.h"
#include "framework/logger_sdldialog.h"
#include "framework/options.h"
#include "framework/os/app_paths.h"
#include "framework/os/display_size.h"
#include "framework/os/file_picker.h"
#include "framework/renderer.h"
#include "framework/renderer_interface.h"
#include "framework/sound_interface.h"
#include "framework/stagestack.h"
#include "library/sp.h"
#include "library/xorshift.h"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <list>
#include <map>
#include <string_view>
#include <vector>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

// SDL_syswm includes windows.h on windows, which does all kinds of polluting
// defines/namespace stuff, so try to avoid that
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <SDL_syswm.h>

// Windows isn't the only thing that pollutes stuff with #defines - X gets in on it too with 'None'
#undef None

// Use physfs to get prefs dir
#include <physfs.h>

// Boost locale for setting the system locale
#include <boost/locale.hpp>

using namespace OpenApoc;

namespace OpenApoc
{

UString Framework::getDataDir() const { return Options::dataPathOption.get(); }

UString Framework::getCDPath() const { return Options::cdPathOption.get(); }

Framework *Framework::instance = nullptr;
class FrameworkPrivate
{
  private:
	friend class Framework;
	bool quitProgram;

	SDL_DisplayMode screenMode;
	SDL_Window *window;
	SDL_GLContext context;

	std::map<UString, std::unique_ptr<RendererFactory>> registeredRenderers;
	std::map<UString, std::unique_ptr<SoundBackendFactory>> registeredSoundBackends;

	std::list<up<Event>> eventQueue;
	std::mutex eventQueueLock;

	StageStack ProgramStages;
	sp<Surface> defaultSurface;
	// Logical world size in window points. scaleSurface blits this to drawableSize.
	Vec2<int> displaySize;
	Vec2<int> windowSize;

	sp<Surface> scaleSurface;
	Vec2<int> drawableSize;
	Vec2<int> lastWindowedSize;
	int uiScale;
	// Mouse input from the OS is ignored while the window is not the focused one.
	bool windowFocused = true;
	// The finger that started the current gesture, held until it lifts. SDL_FingerID is
	// 64-bit and iOS derives it from a UITouch pointer, so it must not be narrowed.
	SDL_FingerID primaryFingerId = 0;
	bool primaryFingerActive = false;

	// iOS only: disambiguates what a touch-synthesized left button press means before
	// forwarding it, since SDL's touch->mouse synthesis (SDL_HINT_TOUCH_MOUSE_EVENTS) fires
	// the down instantly with no notion of a tap, a drag, or a long press. See the
	// SDL_MOUSEBUTTONDOWN/MOTION/UP cases and the post-poll timeout check in
	// translateSdlEvents().
	enum class TouchPressState
	{
		Idle,          // no touch-synthesized press outstanding
		Buffering,     // pressed, not yet released to anyone - still could be any of the three
		ForwardedLeft, // released as a normal (possibly TouchStartedAsPan) left MOUSE_DOWN
		ForwardedRight // released as a long-press-synthesized right MOUSE_DOWN
	};
	TouchPressState touchPressState = TouchPressState::Idle;
	Vec2<int> touchPressOrigin{0, 0};
	Uint32 touchPressStartTicks = 0;

	up<ThreadPool> threadPool;
	up<Harness> harness;

	std::atomic<int> toolTipTimerId = 0;
	up<Event> toolTipTimerEvent;
	sp<Image> toolTipImage;
	Vec2<int> toolTipPosition;

	FrameworkPrivate()
	    : quitProgram(false), window(nullptr), context(0), displaySize(0, 0), windowSize(0, 0),
	      drawableSize(0, 0), lastWindowedSize(kDefaultScreenWidth, kDefaultScreenHeight),
	      uiScale(1)
	{
		int threadPoolSize = Options::threadPoolSizeOption.get();
		if (threadPoolSize > 0)
		{
			LogInfo("Set thread pool size to {0}", threadPoolSize);
		}
		else if (std::thread::hardware_concurrency() != 0)
		{
			threadPoolSize = std::thread::hardware_concurrency();
			LogInfo("Set thread pool size to reported HW concurrency of {0}", threadPoolSize);
		}
		else
		{
			threadPoolSize = 2;
			LogInfo("Failed to get HW concurrency, falling back to pool size {0}", threadPoolSize);
		}

		this->threadPool.reset(new ThreadPool(threadPoolSize));
	}
};

Framework::Framework(const UString programName, bool createWindow)
    : p(new FrameworkPrivate), programName(programName), createWindow(createWindow)
{
	LogInfo("Starting framework");

	if (this->instance)
	{
		LogError("Multiple Framework instances created");
	}

	this->instance = this;

	if (!PHYSFS_isInit())
	{
		if (PHYSFS_init(programName.c_str()) == 0)
		{
			PHYSFS_ErrorCode error = PHYSFS_getLastErrorCode();
			LogError("Failed to init code {0} PHYSFS: {1}", (int)error,
			         PHYSFS_getErrorByCode(error));
		}
	}
#ifdef ANDROID
	SDL_SetHint(SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH, "1");
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
#endif
#ifdef __APPLE__
#if TARGET_OS_IPHONE
	// Let SDL synthesise real mouse events from the primary touch. Every screen under game/ui/
	// that reads raw EVENT_MOUSE_DOWN/UP/MOVE - the equip screens' press-move-release drag and
	// drop included - only ever sees genuine top-level mouse events; a per-Control finger-to-
	// mouse bridge (as forms/control.cpp used to have) never reaches those. translateSdlEvents()
	// below gates the touch-synthesized SDL_MOUSEBUTTONDOWN/UP (SDL_TOUCH_MOUSEID) just enough
	// to add what SDL can't: a tap vs. drag distinction and a long-press right click.
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
#endif
	// Clicking a background window should only raise it, never also act in the game.
	SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "0");
	// Initialize subsystems separately?
	if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER) < 0)
	{
		LogError("Cannot init SDL2");
		LogError("SDL error: {0}", SDL_GetError());
		p->quitProgram = true;
		return;
	}
	if (createWindow)
	{
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
		{
			LogError("Cannot init SDL_VIDEO - \"{0}\"", SDL_GetError());
			p->quitProgram = true;
			return;
		}
	}
	applyAppBundlePathDefaults(programName);
	applyAppBundleDisplayDefaults(programName);
	if (createWindow && !cdPathLooksValid(Options::cdPathOption.get()))
	{
		LogWarning("CD path \"{0}\" is missing; prompting for original game files",
		           Options::cdPathOption.get());
		const UString picked = pickCdPath();
		if (!picked.empty() && cdPathLooksValid(picked))
		{
			Options::cdPathOption.set(picked);
			config().save();
		}
	}
	LogInfo("Loading config\n");
	p->quitProgram = false;
	UString settingsPath(PHYSFS_getPrefDir(PROGRAM_ORGANISATION, PROGRAM_NAME));
	settingsPath += "/settings.cfg";

	UString logPath(PHYSFS_getPrefDir(PROGRAM_ORGANISATION, PROGRAM_NAME));
	std::ifstream portableFile("./portable.txt");
	if (portableFile)
	{
		logPath = ".";
	}

	logPath += "/log.txt";

	enableFileLogger(logPath.c_str());

	Options::dumpOptionsToLog();

	// This is always set, the default being an empty string (which correctly chooses 'system
	// language')
	UString desiredLanguageName;
	if (!Options::languageOption.get().empty())
	{
		desiredLanguageName = Options::languageOption.get();
	}

	LogInfo("Setting up locale \"{0}\"", desiredLanguageName);

	boost::locale::generator gen;

	std::vector<UString> resourcePaths;
	resourcePaths.push_back(Options::cdPathOption.get());
	resourcePaths.push_back(Options::dataPathOption.get());

	for (auto &path : resourcePaths)
	{
		auto langPath = path + "/languages";
		LogInfo("Adding \"{0}\" to language path", langPath);
		gen.add_messages_path(langPath);
	}

	std::vector<UString> translationDomains = {"openapoc"};
	for (auto &domain : translationDomains)
	{
		LogInfo("Adding \"{0}\" to translation domains", domain);
		gen.add_messages_domain(domain);
	}

	std::locale loc = gen(desiredLanguageName);
	std::locale::global(loc);

	auto localeName = std::use_facet<boost::locale::info>(loc).name();
	auto localeLang = std::use_facet<boost::locale::info>(loc).language();
	auto localeCountry = std::use_facet<boost::locale::info>(loc).country();
	auto localeVariant = std::use_facet<boost::locale::info>(loc).variant();
	auto localeEncoding = std::use_facet<boost::locale::info>(loc).encoding();
	auto isUTF8 = std::use_facet<boost::locale::info>(loc).utf8();

	LogInfo("Locale info: Name \"{0}\" language \"{1}\" country \"{2}\" variant \"{3}\" encoding "
	        "\"{4}\" utf8:{5}",
	        localeName.c_str(), localeLang.c_str(), localeCountry.c_str(), localeVariant.c_str(),
	        localeEncoding.c_str(), isUTF8 ? "true" : "false");

	this->language = localeLang;
	this->languageCountry = localeCountry;

	this->data.reset(Data::createData(resourcePaths));

	auto testFile = this->data->fs.open("music");
	if (!testFile)
	{
		LogError("Failed to open \"music\" from the CD - likely the cd couldn't be loaded or paths "
		         "are incorrect if using an extracted CD image");
	}

	auto testFile2 = this->data->fs.open("filedoesntexist");
	if (testFile2)
	{
		LogError("Succeeded in opening \"FileDoesntExist\" - either you have the weirdest filename "
		         "preferences or something is wrong");
	}
	srand(static_cast<unsigned int>(SDL_GetTicks()));

	if (createWindow)
	{
		displayInitialise();
		// SDL_ShowSimpleMessageBox is modal and blocks the thread that calls it until somebody
		// dismisses it, and Logger.dialogLevel defaults to Error -- so under the harness a single
		// LogError deadlocks the main loop forever, taking the harness (which is polled from that
		// same loop) down with it. Nobody is there to click OK on an automated run.
		if (Options::harnessEnable.get())
		{
			LogInfo("Harness enabled: not installing the modal SDL dialog logger");
		}
		else
		{
			enableSDLDialogLogger(p->window);
		}
	}
	audioInitialise(!createWindow);

#if defined(__APPLE__) && TARGET_OS_IPHONE
	if (Options::harnessEnable.get())
	{
		LogWarning("Framework.Harness is disabled on iOS");
	}
#else
	if (Options::harnessEnable.get())
	{
		p->harness.reset(new Harness(Options::harnessPort.get()));
		if (!p->harness->listening())
		{
			p->harness.reset();
		}
	}
#endif
}

Framework::~Framework()
{
	LogInfo("Destroying framework");
	// Stop any audio first, as if you've got ongoing music/samples it could call back into the
	// framework for the threadpool/data read/all kinda of stuff it shouldn't do on a
	// half-destroyed framework
	audioShutdown();
	LogInfo("Stopping threadpool");
	p->threadPool.reset();
	LogInfo("Clearing stages");
	p->ProgramStages.clear();
	LogInfo("Saving config");
	if (config().getBool("Config.Save"))
	{
		revertBundleInternalPathsForSave();
		config().save();
	}

	LogInfo("Shutdown");
	// Make sure we destroy the data implementation before the renderer to ensure any possibly
	// cached images are already destroyed
	this->data.reset();
	if (createWindow)
	{
		displayShutdown();
	}
	LogInfo("SDL shutdown");
	PHYSFS_deinit();
	SDL_Quit();
	instance = nullptr;
}

Framework &Framework::getInstance()
{
	if (!instance)
	{
		LogError("Framework::getInstance() called with no live Framework");
	}
	return *instance;
}
Framework *Framework::tryGetInstance() { return instance; }

void Framework::run(sp<Stage> initialStage)
{
	size_t frameCount = Options::frameLimit.get();
	if (!createWindow)
	{
		LogError("Trying to run framework without window");
		return;
	}
	size_t frame = 0;
	LogInfo("Program loop started");

	auto target_frame_duration =
	    std::chrono::duration<int64_t, std::micro>(1000000 / Options::targetFPS.get());

	p->ProgramStages.push(initialStage);

	this->renderer->setPalette(this->data->loadPalette("xcom3/ufodata/pal_06.dat"));
	auto expected_frame_time = std::chrono::steady_clock::now();

	bool frame_time_limited_warning_shown = false;

	const size_t profileFrames = (size_t)std::max(0, Options::profileFrames.get());
	size_t profileSamples = 0;
	uint64_t profileDrawCalls = 0;
	uint64_t profileSprites = 0;
	std::chrono::steady_clock::duration profileUpdate{};
	std::chrono::steady_clock::duration profileRender{};
	std::chrono::steady_clock::duration profileSwap{};
	std::chrono::steady_clock::duration profileTotal{};

	while (!p->quitProgram)
	{
		auto frame_time_now = std::chrono::steady_clock::now();
		if (expected_frame_time > frame_time_now)
		{
			auto time_to_sleep = expected_frame_time - frame_time_now;
			auto time_to_sleep_us =
			    std::chrono::duration_cast<std::chrono::microseconds>(time_to_sleep);
			LogDebug("sleeping for {0} us", time_to_sleep_us.count());
			std::this_thread::sleep_for(time_to_sleep);
			continue;
		}
		expected_frame_time += target_frame_duration;
		frame++;
		frameNumber++;

		// expected_frame_time only ever advances one frame per iteration, so a single long frame
		// -- city generation on load, a big save -- leaves it arbitrarily far behind wall-clock
		// with no way to catch up: every later iteration sees it already in the past, never
		// sleeps, and TargetFPS silently stops limiting anything for the rest of the session.
		// That is why this fired on essentially every launch: it was reporting the load hitch,
		// not a steady-state pacing problem. Resynchronise rather than accumulate a debt that
		// cannot be paid.
		if (frame_time_now > expected_frame_time + 5 * target_frame_duration)
		{
			expected_frame_time = frame_time_now + target_frame_duration;
		}
		if (!frame_time_limited_warning_shown &&
		    frame_time_now > expected_frame_time + 5 * target_frame_duration)
		{
			frame_time_limited_warning_shown = true;
			LogWarning("Over 5 frames behind - likely vsync limited?");
		}

		if (p->harness)
		{
			p->harness->poll(*this);
		}
		processEvents();

		if (p->ProgramStages.isEmpty())
		{
			break;
		}
		const auto profileFrameStart = std::chrono::steady_clock::now();
		{
			p->ProgramStages.current()->update();
		}
		const auto profileUpdateEnd = std::chrono::steady_clock::now();
		auto profileSwapStart = profileUpdateEnd;

		// Iterate a copy. REPLACEALL/QUIT below clear the stage stack, which destroys stages and
		// everything they own; anything in that teardown that queues another stage command would
		// append to this very vector mid-iteration and invalidate the range-for. Copying keeps
		// the existing semantics -- commands raised while processing this batch are discarded by
		// the clear() below, exactly as before -- without the undefined behaviour.
		const auto commandsThisFrame = stageCommands;
		for (const StageCmd &cmd : commandsThisFrame)
		{
			switch (cmd.cmd)
			{
				case StageCmd::Command::CONTINUE:
					break;
				case StageCmd::Command::REPLACE:
					p->ProgramStages.pop();
					p->ProgramStages.push(cmd.nextStage);
					break;
				case StageCmd::Command::REPLACEALL:
					p->ProgramStages.clear();
					p->ProgramStages.push(cmd.nextStage);
					break;
				case StageCmd::Command::PUSH:
					p->ProgramStages.push(cmd.nextStage);
					break;
				case StageCmd::Command::POP:
					p->ProgramStages.pop();
					break;
				case StageCmd::Command::QUIT:
					p->quitProgram = true;
					p->ProgramStages.clear();
					break;
			}
			if (p->quitProgram)
			{
				break;
			}
		}
		stageCommands.clear();

		auto surface = p->scaleSurface ? p->scaleSurface : p->defaultSurface;
		RendererSurfaceBinding b(*this->renderer, surface);
		{
			this->renderer->clear();
		}
		if (!p->ProgramStages.isEmpty())
		{
			p->ProgramStages.current()->render();
			if (p->toolTipImage)
			{
				renderer->draw(p->toolTipImage, p->toolTipPosition);
			}
			this->cursor->render();
			if (p->scaleSurface)
			{
				RendererSurfaceBinding scaleBind(*this->renderer, p->defaultSurface);
				this->renderer->clear();
				this->renderer->drawScaled(p->scaleSurface, {0, 0}, p->drawableSize);
			}
			{
				this->renderer->flush();
				this->renderer->newFrame();
				profileSwapStart = std::chrono::steady_clock::now();
				SDL_GL_SwapWindow(p->window);
			}
		}
		if (profileFrames)
		{
			const auto profileFrameEnd = std::chrono::steady_clock::now();
			profileUpdate += profileUpdateEnd - profileFrameStart;
			profileRender += profileSwapStart - profileUpdateEnd;
			profileSwap += profileFrameEnd - profileSwapStart;
			profileTotal += profileFrameEnd - profileFrameStart;
			profileDrawCalls += this->renderer->takeDrawCallCount();
			profileSprites += this->renderer->takeSpriteCount();
			if (++profileSamples >= profileFrames)
			{
				const auto avgMs = [profileSamples](std::chrono::steady_clock::duration d)
				{
					return std::chrono::duration<double, std::milli>(d).count() /
					       (double)profileSamples;
				};
				LogWarning("Frame profile over {0} frames: update {1:.2f} ms, draw {2:.2f} ms, "
				           "swap {3:.2f} ms, busy {4:.2f} ms ({5:.1f} fps if uncapped), "
				           "{6} draw calls + {7} sprites/frame, display {8} drawable {9} "
				           "uiScale {10}",
				           (unsigned long long)profileSamples, avgMs(profileUpdate),
				           avgMs(profileRender), avgMs(profileSwap), avgMs(profileTotal),
				           1000.0 / avgMs(profileTotal),
				           (unsigned long long)(profileDrawCalls / profileSamples),
				           (unsigned long long)(profileSprites / profileSamples), p->displaySize,
				           p->drawableSize, p->uiScale);
				profileSamples = 0;
				profileDrawCalls = 0;
				profileSprites = 0;
				profileUpdate = {};
				profileRender = {};
				profileSwap = {};
				profileTotal = {};
			}
		}

		if (frameCount && frame == frameCount)
		{
			LogWarning("Quitting hitting frame count limit of {0}", (unsigned long long)frame);
			p->quitProgram = true;
		}
	}
}

void Framework::processEvents()
{
	if (p->ProgramStages.isEmpty())
	{
		p->quitProgram = true;
		return;
	}

	// TODO: Consider threading the translation
	translateSdlEvents();

	while (p->eventQueue.size() > 0 && !p->ProgramStages.isEmpty())
	{
		up<Event> e;
		{
			std::lock_guard<std::mutex> l(p->eventQueueLock);
			e = std::move(p->eventQueue.front());
			p->eventQueue.pop_front();
		}
		if (!e)
		{
			LogError("Invalid event on queue");
			continue;
		}
		this->cursor->eventOccured(e.get());
		if (e->type() == EVENT_KEY_DOWN)
		{
			if (e->keyboard().KeyCode == SDLK_PRINTSCREEN)
			{
				int screenshotId = 0;
				UString screenshotName;
				do
				{
					screenshotName = format("screenshot{0:03}.png", screenshotId);
					screenshotId++;
				} while (fs::exists(fs::path(screenshotName)));
				LogWarning("Writing screenshot to \"{0}\"", screenshotName);
				if (!p->defaultSurface->rendererPrivateData)
				{
					LogWarning("No renderer data on surface - nothing drawn yet?");
				}
				else
				{
					auto img = p->defaultSurface->rendererPrivateData->readBack();
					if (!img)
					{
						LogWarning("No image returned");
					}
					else
					{
						this->threadPoolTaskEnqueue(
						    [img, screenshotName]
						    {
							    auto ret = fw().data->writeImage(screenshotName, img);
							    if (!ret)
							    {
								    LogWarning("Failed to write screenshot");
							    }
							    else
							    {
								    LogInfo("Wrote screenshot to \"{0}\"", screenshotName);
							    }
						    });
					}
				}
			}
		}
		switch (e->type())
		{
			case EVENT_WINDOW_CLOSED:
				shutdownFramework();
				return;
			default:
				p->ProgramStages.current()->eventOccurred(e.get());
				break;
		}
	}
	/* Drop any events left in the list, as it's possible an event caused the last stage to pop
	 * with events outstanding, but they can safely be ignored as we're quitting anyway */
	{
		std::lock_guard<std::mutex> l(p->eventQueueLock);
		p->eventQueue.clear();
	}
}

void Framework::pushEvent(up<Event> e)
{
	std::lock_guard<std::mutex> l(p->eventQueueLock);
	p->eventQueue.push_back(std::move(e));
}

void Framework::pushEvent(Event *e) { this->pushEvent(up<Event>(e)); }

void Framework::translateSdlEvents()
{
	SDL_Event e;
	Event *fwE;
	bool touch_events_enabled = Options::optionEnableTouchEvents.get();

#if defined(__APPLE__) && TARGET_OS_IPHONE
	// A touch-synthesized left press (SDL_HINT_TOUCH_MOUSE_EVENTS, which() == SDL_TOUCH_MOUSEID)
	// is held back until we know it isn't the start of a pan: it's either released first (a tap
	// - release DOWN immediately followed by UP), it moves past kTouchTapSlopPixels before that
	// (a drag - release DOWN tagged TouchStartedAsPan so CityView/BattleView know not to also
	// select/attack from it), or it sits still past kTouchLongPressMs (a long press - release it
	// as a right button instead, since SDL's synthesis only ever presses the left one).
	static constexpr int kTouchTapSlopPixels = 12;
	static constexpr Uint32 kTouchLongPressMs = 450;
	auto releaseBufferedTouchPress = [this](Uint8 button, bool startedAsPan)
	{
		Event *downEvent = new MouseEvent(EVENT_MOUSE_DOWN);
		downEvent->mouse().X = p->touchPressOrigin.x;
		downEvent->mouse().Y = p->touchPressOrigin.y;
		downEvent->mouse().DeltaX = 0;
		downEvent->mouse().DeltaY = 0;
		downEvent->mouse().WheelVertical = 0;
		downEvent->mouse().WheelHorizontal = 0;
		downEvent->mouse().Button = SDL_BUTTON(button);
		downEvent->mouse().TouchStartedAsPan = startedAsPan;
		pushEvent(up<Event>(downEvent));
		p->touchPressState = button == SDL_BUTTON_RIGHT
		                         ? FrameworkPrivate::TouchPressState::ForwardedRight
		                         : FrameworkPrivate::TouchPressState::ForwardedLeft;
	};
#endif

	while (SDL_PollEvent(&e))
	{
		// A background window must not be playable: neither the click that raises it nor
		// the pointer passing over it should reach the game. Releases and key-ups are let
		// through so a drag or held key interrupted by an app switch cannot stick down.
		if (!p->windowFocused)
		{
			switch (e.type)
			{
				case SDL_MOUSEMOTION:
				case SDL_MOUSEWHEEL:
				case SDL_MOUSEBUTTONDOWN:
				case SDL_FINGERDOWN:
				case SDL_FINGERMOTION:
					continue;
				default:
					break;
			}
		}
		switch (e.type)
		{
			case SDL_QUIT:
				fwE = new DisplayEvent(EVENT_WINDOW_CLOSED);
				pushEvent(up<Event>(fwE));
				break;
			case SDL_JOYDEVICEADDED:
			case SDL_JOYDEVICEREMOVED:
				// FIXME: Do nothing?
				break;
			case SDL_KEYDOWN:
#if !(defined(__APPLE__) && TARGET_OS_IPHONE)
				if (!e.key.repeat &&
				    (e.key.keysym.sym == SDLK_F11 ||
				     (e.key.keysym.sym == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT))))
				{
					displayToggleFullscreen();
					break;
				}
#endif
				fwE = new KeyboardEvent(EVENT_KEY_DOWN);
				fwE->keyboard().KeyCode = e.key.keysym.sym;
				fwE->keyboard().ScanCode = e.key.keysym.scancode;
				fwE->keyboard().Modifiers = e.key.keysym.mod;
				pushEvent(up<Event>(fwE));
				break;
			case SDL_KEYUP:
				fwE = new KeyboardEvent(EVENT_KEY_UP);
				fwE->keyboard().KeyCode = e.key.keysym.sym;
				fwE->keyboard().ScanCode = e.key.keysym.scancode;
				fwE->keyboard().Modifiers = e.key.keysym.mod;
				pushEvent(up<Event>(fwE));
				break;
			case SDL_TEXTINPUT:
				fwE = new TextEvent();
				fwE->text().Input = e.text.text;
				pushEvent(up<Event>(fwE));
				break;
			case SDL_TEXTEDITING:
				// FIXME: Do nothing?
				break;
			case SDL_MOUSEMOTION:
#if defined(__APPLE__) && TARGET_OS_IPHONE
				if (e.motion.which == SDL_TOUCH_MOUSEID &&
				    p->touchPressState == FrameworkPrivate::TouchPressState::Buffering)
				{
					int x = coordWindowToDisplayX(e.motion.x);
					int y = coordWindowToDisplayY(e.motion.y);
					int dx = x - p->touchPressOrigin.x;
					int dy = y - p->touchPressOrigin.y;
					if (dx * dx + dy * dy <= kTouchTapSlopPixels * kTouchTapSlopPixels)
					{
						// Still ambiguous - could yet be a tap or a long press. Don't forward
						// motion for a press we haven't released as a mouse-down yet.
						break;
					}
					releaseBufferedTouchPress(SDL_BUTTON_LEFT, /*startedAsPan=*/true);
					// Fall through and forward this same motion like any other mouse move.
				}
#endif
				fwE = new MouseEvent(EVENT_MOUSE_MOVE);
				fwE->mouse().X = coordWindowToDisplayX(e.motion.x);
				fwE->mouse().Y = coordWindowToDisplayY(e.motion.y);
				fwE->mouse().DeltaX = e.motion.xrel;
				fwE->mouse().DeltaY = e.motion.yrel;
				fwE->mouse().WheelVertical = 0;   // These should be handled
				fwE->mouse().WheelHorizontal = 0; // in a separate event
				fwE->mouse().Button = e.motion.state;
				pushEvent(up<Event>(fwE));
				break;
			case SDL_MOUSEWHEEL:
				// FIXME: Check these values for sanity
				fwE = new MouseEvent(EVENT_MOUSE_SCROLL);
				// Since I'm using some variables that are not used anywhere else,
				// this code should be in its own small block.
				{
					int mx, my;
					fwE->mouse().Button = SDL_GetMouseState(&mx, &my);
					fwE->mouse().X = coordWindowToDisplayX(mx);
					fwE->mouse().Y = coordWindowToDisplayY(my);
					fwE->mouse().DeltaX = 0; // FIXME: This might cause problems?
					fwE->mouse().DeltaY = 0;
					fwE->mouse().WheelVertical = e.wheel.y;
					fwE->mouse().WheelHorizontal = e.wheel.x;
				}
				pushEvent(up<Event>(fwE));
				break;
			case SDL_MOUSEBUTTONDOWN:
#if defined(__APPLE__) && TARGET_OS_IPHONE
				if (e.button.which == SDL_TOUCH_MOUSEID)
				{
					// Hold it - don't know yet whether this becomes a tap, a drag, or a long
					// press. See releaseBufferedTouchPress() above and SDL_MOUSEBUTTONUP below.
					p->touchPressState = FrameworkPrivate::TouchPressState::Buffering;
					p->touchPressOrigin = {coordWindowToDisplayX(e.button.x),
					                        coordWindowToDisplayY(e.button.y)};
					p->touchPressStartTicks = e.button.timestamp;
					break;
				}
#endif
				fwE = new MouseEvent(EVENT_MOUSE_DOWN);
				fwE->mouse().X = coordWindowToDisplayX(e.button.x);
				fwE->mouse().Y = coordWindowToDisplayY(e.button.y);
				fwE->mouse().DeltaX = 0; // FIXME: This might cause problems?
				fwE->mouse().DeltaY = 0;
				fwE->mouse().WheelVertical = 0;
				fwE->mouse().WheelHorizontal = 0;
				fwE->mouse().Button = SDL_BUTTON(e.button.button);
				pushEvent(up<Event>(fwE));
				break;
			case SDL_MOUSEBUTTONUP:
#if defined(__APPLE__) && TARGET_OS_IPHONE
				if (e.button.which == SDL_TOUCH_MOUSEID)
				{
					if (p->touchPressState == FrameworkPrivate::TouchPressState::Buffering)
					{
						// Released before slop or the long-press timeout: an ordinary tap.
						// Release the held DOWN now, immediately followed by this UP.
						releaseBufferedTouchPress(SDL_BUTTON_LEFT, /*startedAsPan=*/false);
					}
					if (p->touchPressState == FrameworkPrivate::TouchPressState::Idle)
					{
						// No matching down was ever buffered (e.g. the window lost focus
						// mid-press and reset the gesture) - nothing to release.
						break;
					}
					Uint8 button = p->touchPressState ==
					                       FrameworkPrivate::TouchPressState::ForwardedRight
					                   ? SDL_BUTTON_RIGHT
					                   : SDL_BUTTON_LEFT;
					fwE = new MouseEvent(EVENT_MOUSE_UP);
					fwE->mouse().X = coordWindowToDisplayX(e.button.x);
					fwE->mouse().Y = coordWindowToDisplayY(e.button.y);
					fwE->mouse().DeltaX = 0;
					fwE->mouse().DeltaY = 0;
					fwE->mouse().WheelVertical = 0;
					fwE->mouse().WheelHorizontal = 0;
					fwE->mouse().Button = SDL_BUTTON(button);
					pushEvent(up<Event>(fwE));
					p->touchPressState = FrameworkPrivate::TouchPressState::Idle;
					break;
				}
#endif
				fwE = new MouseEvent(EVENT_MOUSE_UP);
				fwE->mouse().X = coordWindowToDisplayX(e.button.x);
				fwE->mouse().Y = coordWindowToDisplayY(e.button.y);
				fwE->mouse().DeltaX = 0; // FIXME: This might cause problems?
				fwE->mouse().DeltaY = 0;
				fwE->mouse().WheelVertical = 0;
				fwE->mouse().WheelHorizontal = 0;
				fwE->mouse().Button = SDL_BUTTON(e.button.button);
				pushEvent(up<Event>(fwE));
				break;
			case SDL_FINGERDOWN:
				if (!touch_events_enabled)
					break;
				if (!p->primaryFingerActive)
				{
					p->primaryFingerActive = true;
					p->primaryFingerId = e.tfinger.fingerId;
				}
				fwE = new FingerEvent(EVENT_FINGER_DOWN);
				fwE->finger().X = static_cast<int>(e.tfinger.x * displayGetWidth());
				fwE->finger().Y = static_cast<int>(e.tfinger.y * displayGetHeight());
				fwE->finger().DeltaX = static_cast<int>(e.tfinger.dx * displayGetWidth());
				fwE->finger().DeltaY = static_cast<int>(e.tfinger.dy * displayGetHeight());
				fwE->finger().Id = e.tfinger.fingerId;
				fwE->finger().IsPrimary =
				    p->primaryFingerActive && e.tfinger.fingerId == p->primaryFingerId;
				pushEvent(up<Event>(fwE));
				break;
			case SDL_FINGERUP:
				if (!touch_events_enabled)
					break;
				fwE = new FingerEvent(EVENT_FINGER_UP);
				fwE->finger().X = static_cast<int>(e.tfinger.x * displayGetWidth());
				fwE->finger().Y = static_cast<int>(e.tfinger.y * displayGetHeight());
				fwE->finger().DeltaX = static_cast<int>(e.tfinger.dx * displayGetWidth());
				fwE->finger().DeltaY = static_cast<int>(e.tfinger.dy * displayGetHeight());
				fwE->finger().Id = e.tfinger.fingerId;
				fwE->finger().IsPrimary =
				    p->primaryFingerActive && e.tfinger.fingerId == p->primaryFingerId;
				pushEvent(up<Event>(fwE));
				if (p->primaryFingerActive && e.tfinger.fingerId == p->primaryFingerId)
				{
					p->primaryFingerActive = false;
				}
				break;
			case SDL_FINGERMOTION:
				if (!touch_events_enabled)
					break;
				fwE = new FingerEvent(EVENT_FINGER_MOVE);
				fwE->finger().X = static_cast<int>(e.tfinger.x * displayGetWidth());
				fwE->finger().Y = static_cast<int>(e.tfinger.y * displayGetHeight());
				fwE->finger().DeltaX = static_cast<int>(e.tfinger.dx * displayGetWidth());
				fwE->finger().DeltaY = static_cast<int>(e.tfinger.dy * displayGetHeight());
				fwE->finger().Id = e.tfinger.fingerId;
				fwE->finger().IsPrimary =
				    p->primaryFingerActive && e.tfinger.fingerId == p->primaryFingerId;
				pushEvent(up<Event>(fwE));
				break;
			case SDL_WINDOWEVENT:
				// Window events get special treatment
				switch (e.window.event)
				{
					case SDL_WINDOWEVENT_FOCUS_GAINED:
						p->windowFocused = true;
						fwE = new DisplayEvent(EVENT_WINDOW_ACTIVATE);
						fwE->display().X = 0;
						fwE->display().Y = 0;
						SDL_GetWindowSize(p->window, &(fwE->display().Width),
						                  &(fwE->display().Height));
						fwE->display().Active = true;
						pushEvent(up<Event>(fwE));
						break;
					case SDL_WINDOWEVENT_FOCUS_LOST:
						p->windowFocused = false;
						p->primaryFingerActive = false;
#if defined(__APPLE__) && TARGET_OS_IPHONE
						// A backgrounded app isn't guaranteed a matching SDL_MOUSEBUTTONUP for a
						// press that was still buffered - don't leave the gate stuck open.
						p->touchPressState = FrameworkPrivate::TouchPressState::Idle;
#endif
						fwE = new DisplayEvent(EVENT_WINDOW_DEACTIVATE);
						fwE->display().X = 0;
						fwE->display().Y = 0;
						SDL_GetWindowSize(p->window, &(fwE->display().Width),
						                  &(fwE->display().Height));
						fwE->display().Active = false;
						pushEvent(up<Event>(fwE));
						break;
					case SDL_WINDOWEVENT_SIZE_CHANGED:
					case SDL_WINDOWEVENT_RESIZED:
						displayRefreshSize();
						fwE = new DisplayEvent(EVENT_WINDOW_RESIZE);
						fwE->display().X = 0;
						fwE->display().Y = 0;
						fwE->display().Width = p->displaySize.x;
						fwE->display().Height = p->displaySize.y;
						fwE->display().Active = true;
						pushEvent(up<Event>(fwE));
						break;
					case SDL_WINDOWEVENT_HIDDEN:
					case SDL_WINDOWEVENT_MINIMIZED:
					case SDL_WINDOWEVENT_LEAVE:
						// FIXME: Check if we should react this way for each of those events
						// FIXME: Check if we're missing some of the events
						fwE = new DisplayEvent(EVENT_WINDOW_DEACTIVATE);
						fwE->display().X = 0;
						fwE->display().Y = 0;
						// FIXME: Is this even necessary?
						SDL_GetWindowSize(p->window, &(fwE->display().Width),
						                  &(fwE->display().Height));
						fwE->display().Active = false;
						pushEvent(up<Event>(fwE));
						break;
					case SDL_WINDOWEVENT_SHOWN:
					case SDL_WINDOWEVENT_EXPOSED:
					case SDL_WINDOWEVENT_RESTORED:
					case SDL_WINDOWEVENT_ENTER:
						// FIXME: Should we handle all these events as "aaand we're back"
						// events?
						fwE = new DisplayEvent(EVENT_WINDOW_ACTIVATE);
						fwE->display().X = 0;
						fwE->display().Y = 0;
						// FIXME: Is this even necessary?
						SDL_GetWindowSize(p->window, &(fwE->display().Width),
						                  &(fwE->display().Height));
						fwE->display().Active = false;
						pushEvent(up<Event>(fwE));
						break;
					case SDL_WINDOWEVENT_CLOSE:
						// Closing a window will be a "quit" event.
						e.type = SDL_QUIT;
						SDL_PushEvent(&e);
						break;
				}
				break;
			default:
				break;
		}
	}

#if defined(__APPLE__) && TARGET_OS_IPHONE
	// The finger may simply be held still with no new SDL event arriving at all, so the
	// long-press timeout can't be detected from inside the poll loop above - check it once per
	// drain instead.
	if (p->touchPressState == FrameworkPrivate::TouchPressState::Buffering &&
	    SDL_GetTicks() - p->touchPressStartTicks >= kTouchLongPressMs)
	{
		releaseBufferedTouchPress(SDL_BUTTON_RIGHT, /*startedAsPan=*/false);
	}
#endif
}

void Framework::shutdownFramework()
{
	LogInfo("Shutdown framework");
	p->ProgramStages.clear();
	p->quitProgram = true;
}

enum class ScreenMode
{
	Unknown,
	Windowed,
	FullScreen,
	Borderless
};

static ScreenMode optionsScreenMode()
{
	constexpr std::array<std::pair<std::string_view, ScreenMode>, 3> mode_names = {
	    {{"windowed", ScreenMode::Windowed},
	     {"fullscreen", ScreenMode::FullScreen},
	     {"borderless", ScreenMode::Borderless}}};

	for (const auto &mode_name : mode_names)
	{
		if (Options::screenModeOption.get() == mode_name.first)
		{
			return mode_name.second;
		}
	}
	return ScreenMode::Unknown;
}

void Framework::displayInitialise()
{
	if (!this->createWindow)
	{
		return;
	}
	LogInfo("Init display");
	int display_flags = SDL_WINDOW_OPENGL;
#ifdef SDL_WINDOW_ALLOW_HIGHDPI
	display_flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
#if defined(__APPLE__) && TARGET_OS_IPHONE
	display_flags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
#endif
	// GL_2_0 cannot run on a core profile -- it feeds GL from client memory and its shaders
	// are #version 110 -- so this stays opt-in rather than being inferred from the renderer list.
	[[maybe_unused]] const bool requestCoreProfile = Options::glProfileOption.get() == "core";
#ifdef OPENAPOC_GLES
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	// Request context version 3.0
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
	// This used to be guarded by SDL_OPENGL_CORE, which nothing in the build ever defines --
	// so no core profile was ever requested, and macOS (which offers 3.2 and 4.1 through a
	// core profile only) always fell back to a legacy 2.1 context with neither ES3 nor
	// GL_ARB_ES3_compatibility. That is why GLES_3_0 could not initialise there.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
	                    requestCoreProfile ? SDL_GL_CONTEXT_PROFILE_CORE : 0);
	// 4.1 is the ceiling on macOS. Asking for less there yields 3.2/GLSL 1.50, which has no
	// explicit attribute locations -- the GLES_3_0 shaders would not compile.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, requestCoreProfile ? 4 : 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, requestCoreProfile ? 1 : 0);
#endif
#ifdef DEBUG_RENDERER
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
	// Request RGBA8888 - change if needed
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetSwapInterval(Options::swapInterval.get());

	ScreenMode mode = optionsScreenMode();
	if (mode == ScreenMode::Unknown)
	{
		LogError("Unknown screen mode specified: {{{0}}}", Options::screenModeOption.get());
		mode = ScreenMode::Windowed;
	}

	int displayNumber = Options::screenDisplayNumberOption.get();
	if (displayNumber >= SDL_GetNumVideoDisplays())
	{
		LogWarning("Requested display number ({0}) does not exist. Using display 0", displayNumber);
		displayNumber = 0;
	}

	SDL_DisplayMode desktop{};
	if (SDL_GetDesktopDisplayMode(displayNumber, &desktop) != 0 || desktop.w <= 0 ||
	    desktop.h <= 0)
	{
		LogWarning("Could not read desktop mode for display {0}: {1}", displayNumber,
		           SDL_GetError());
		desktop.w = kDefaultScreenWidth;
		desktop.h = kDefaultScreenHeight;
	}

	const int requestedW = Options::screenWidthOption.get();
	const int requestedH = Options::screenHeightOption.get();
	Vec2<int> resolved = resolveWindowSize(requestedW, requestedH, desktop.w, desktop.h);
	if (requestedW <= 0 || requestedH <= 0)
	{
		LogInfo("Using desktop size {0} for requested {{{1},{2}}}", resolved, requestedW,
		        requestedH);
	}

	if (mode == ScreenMode::FullScreen)
	{
		SDL_DisplayMode want{};
		want.w = resolved.x;
		want.h = resolved.y;
		want.format = desktop.format;
		want.refresh_rate = desktop.refresh_rate;
		SDL_DisplayMode closest{};
		if (!SDL_GetClosestDisplayMode(displayNumber, &want, &closest))
		{
			LogWarning("No exclusive mode near {0}, using borderless desktop", resolved);
			mode = ScreenMode::Borderless;
		}
		else
		{
			resolved = {closest.w, closest.h};
			display_flags |= SDL_WINDOW_FULLSCREEN;
		}
	}
	if (mode == ScreenMode::Borderless)
	{
		display_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
#if !(defined(__APPLE__) && TARGET_OS_IPHONE)
	if (mode == ScreenMode::Windowed)
	{
		display_flags |= SDL_WINDOW_RESIZABLE;
	}
#endif

	if (mode == ScreenMode::Windowed)
	{
		p->lastWindowedSize = resolved;
	}

	p->window =
	    SDL_CreateWindow("OpenApoc", SDL_WINDOWPOS_UNDEFINED_DISPLAY(displayNumber),
	                     SDL_WINDOWPOS_UNDEFINED_DISPLAY(displayNumber), resolved.x, resolved.y,
	                     display_flags);

	if (!p->window)
	{
		LogError("Failed to create window \"{0}\"", SDL_GetError());
		exit(1);
	}
	p->windowFocused = (SDL_GetWindowFlags(p->window) & SDL_WINDOW_INPUT_FOCUS) != 0;

	p->context = SDL_GL_CreateContext(p->window);
	if (!p->context)
	{
#ifdef OPENAPOC_GLES
		LogError("Failed to create OpenGL ES 3.0 context! [SDLerror: {0}]", SDL_GetError());
		SDL_DestroyWindow(p->window);
		exit(1);
#else
		// The first request is for a version macOS never grants, so this retry is the normal
		// path there, not a fault. A genuine failure is the LogError + exit(1) just below.
		LogInfo("GL context request unsupported by driver, retrying with a legacy context "
		        "[SDLError: {0}]",
		        SDL_GetError());
		LogInfo("Attempting to create context by lowering the requested version");
		// A core profile mask left set here would make the retry ask for "2.0 core",
		// which is not a profile any driver can grant.
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		p->context = SDL_GL_CreateContext(p->window);
		if (!p->context)
		{
			LogError("Failed to create GL context! [SDLerror: {0}]", SDL_GetError());
			SDL_DestroyWindow(p->window);
			exit(1);
		}
#endif
	}
	// Output the context parameters
	LogInfo("Created OpenGL context, parameters:");
	int value;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &value);
	UString profileType;
	switch (value)
	{
		case SDL_GL_CONTEXT_PROFILE_ES:
			profileType = "ES";
			break;
		case SDL_GL_CONTEXT_PROFILE_CORE:
			profileType = "Core";
			break;
		case SDL_GL_CONTEXT_PROFILE_COMPATIBILITY:
			profileType = "Compatibility";
			break;
		default:
			profileType = "Unknown";
	}
	LogInfo("  Context profile: {0}", profileType);
	int ctxMajor, ctxMinor;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &ctxMajor);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &ctxMinor);
	LogInfo("  Context version: {0}.{1}", ctxMajor, ctxMinor);
	int bitsRed, bitsGreen, bitsBlue, bitsAlpha;
	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &bitsRed);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &bitsGreen);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &bitsBlue);
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &bitsAlpha);
	LogInfo("  RGBA bits: {0}-{1}-{2}-{3}", bitsRed, bitsGreen, bitsBlue, bitsAlpha);
	SDL_GL_MakeCurrent(p->window, p->context); // for good measure?
	SDL_ShowCursor(SDL_DISABLE);

	p->registeredRenderers["GLES_3_0"].reset(getGLES30RendererFactory());
#if !defined(__ANDROID__) && !defined(OPENAPOC_GLES)
	p->registeredRenderers["GL_2_0"].reset(getGL20RendererFactory());
#endif

	for (auto &rendererName : split(Options::renderersOption.get(), ":"))
	{
		auto rendererFactory = p->registeredRenderers.find(rendererName);
		if (rendererFactory == p->registeredRenderers.end())
		{
			LogInfo("Renderer \"{0}\" not in supported list", rendererName);
			continue;
		}
		Renderer *r = rendererFactory->second->create();
		if (!r)
		{
			LogInfo("Renderer \"{0}\" failed to init", rendererName);
			continue;
		}
		this->renderer.reset(r);
		LogInfo("Using renderer: {0}", this->renderer->getName());
		break;
	}
	if (!this->renderer)
	{
		LogError("No functional renderer found");
		abort();
	}
	this->p->defaultSurface = this->renderer->getDefaultSurface();

	setMouseGrab();
	displayRefreshSize();
	this->cursor.reset(new ApocCursor(this->data->loadPalette("xcom3/tacdata/tactical.pal")));
}

void Framework::displayShutdown()
{
	this->cursor.reset();
	if (!p->window)
	{
		return;
	}
	LogInfo("Shutdown Display");
	p->defaultSurface.reset();
	renderer.reset();

	SDL_GL_DeleteContext(p->context);
	SDL_DestroyWindow(p->window);
}

int Framework::displayGetWidth() { return p->displaySize.x; }

int Framework::displayGetHeight() { return p->displaySize.y; }

Vec2<int> Framework::displayGetSize() { return p->displaySize; }

void Framework::displaySetSize(Vec2<int> size)
{
	if (!p->window)
	{
		return;
	}
	SDL_SetWindowSize(p->window, std::max(kMinScreenWidth, size.x),
	                  std::max(kMinScreenHeight, size.y));
	displayRefreshSize();
}

int Framework::uiGetScale() const { return std::max(kMinUiScale, p->uiScale); }

void Framework::displayRefreshSize()
{
	if (!p->window)
	{
		return;
	}

	int width = 0;
	int height = 0;
	SDL_GetWindowSize(p->window, &width, &height);
	int drawW = width;
	int drawH = height;
	SDL_GL_GetDrawableSize(p->window, &drawW, &drawH);

	const Vec2<int> newWindow{width, height};
	const Vec2<int> newLogical{std::max(1, width), std::max(1, height)};
	const Vec2<int> newDrawable{std::max(1, drawW), std::max(1, drawH)};
	const bool autoScale = Options::screenAutoScale.get();
	// Tiles and UI layout use window points. HiDPI backing-store pixels are an
	// upscale blit, not extra world work.
	const Vec2<int> newDisplay =
	    computeDisplaySize(newLogical, Options::screenScaleXOption.get(),
	                       Options::screenScaleYOption.get(), autoScale);
	const int newUiScale =
	    computeUiScale(newDisplay.x, Options::screenUiScaleOption.get(), autoScale);

	const bool sizeChanged = newWindow != p->windowSize || newDrawable != p->drawableSize ||
	                         newDisplay != p->displaySize || newUiScale != p->uiScale;

	p->windowSize = newWindow;
	p->drawableSize = newDrawable;
	p->displaySize = newDisplay;
	p->uiScale = newUiScale;
	if (optionsScreenMode() == ScreenMode::Windowed && newWindow.x > 0 && newWindow.y > 0)
	{
		p->lastWindowedSize = newWindow;
	}

	if (p->drawableSize != p->windowSize)
	{
		LogInfo("HiDPI drawable size {0} from window size {1}", p->drawableSize, p->windowSize);
	}
	if (newUiScale > 1)
	{
		LogInfo("UI scale {0}x on display {1} (forms stay {2})", newUiScale, p->displaySize,
		        uiLogicalSize(p->displaySize, newUiScale));
	}

	if (!sizeChanged)
	{
		return;
	}

	if (p->defaultSurface)
	{
		p->defaultSurface->size = {(unsigned)newDrawable.x, (unsigned)newDrawable.y};
		if (p->defaultSurface->rendererPrivateData)
		{
			p->defaultSurface->rendererPrivateData->resize(
			    {(unsigned)newDrawable.x, (unsigned)newDrawable.y});
		}
	}

	const bool wantScale = newDisplay != newDrawable;
	if (wantScale)
	{
		if (!p->scaleSurface ||
		    p->scaleSurface->size != Vec2<unsigned int>{(unsigned)newDisplay.x,
		                                                (unsigned)newDisplay.y})
		{
			LogInfo("Scaling from {0} to {1}", newDisplay, newDrawable);
			p->scaleSurface = mksp<Surface>(newDisplay);
		}
	}
	else
	{
		p->scaleSurface.reset();
	}
}

void Framework::displayToggleFullscreen()
{
#if defined(__APPLE__) && TARGET_OS_IPHONE
	return;
#else
	if (!p->window)
	{
		return;
	}
	ScreenMode mode = optionsScreenMode();
	if (mode == ScreenMode::Windowed)
	{
		SDL_GetWindowSize(p->window, &p->lastWindowedSize.x, &p->lastWindowedSize.y);
		if (SDL_SetWindowFullscreen(p->window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
		{
			LogWarning("Could not enter borderless fullscreen: {0}", SDL_GetError());
			return;
		}
		Options::screenModeOption.set("borderless");
	}
	else
	{
		if (SDL_SetWindowFullscreen(p->window, 0) != 0)
		{
			LogWarning("Could not leave fullscreen: {0}", SDL_GetError());
			return;
		}
		SDL_SetWindowSize(p->window, std::max(kMinScreenWidth, p->lastWindowedSize.x),
		                  std::max(kMinScreenHeight, p->lastWindowedSize.y));
		Options::screenModeOption.set("windowed");
	}
	displayRefreshSize();
#endif
}

int Framework::coordWindowToDisplayX(int x) const
{
	return (float)x / p->windowSize.x * p->displaySize.x;
}

int Framework::coordWindowToDisplayY(int y) const
{
	return (float)y / p->windowSize.y * p->displaySize.y;
}

Vec2<int> Framework::coordWindowsToDisplay(const Vec2<int> &coord) const
{
	return Vec2<int>(coordWindowToDisplayX(coord.x), coordWindowToDisplayY(coord.y));
}

bool Framework::displayHasWindow() const
{
	if (createWindow == false)
		return false;
	if (!p->window)
		return false;
	return true;
}

void Framework::displaySetTitle(UString NewTitle)
{
	if (p->window)
	{
		SDL_SetWindowTitle(p->window, NewTitle.c_str());
	}
}

void Framework::displaySetIcon(sp<RGBImage> image)
{
	if (!p->window)
	{
		return;
	}
#ifdef _WIN32
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	SDL_GetWindowWMInfo(p->window, &info);
	HINSTANCE handle = GetModuleHandle(NULL);
	HICON icon = LoadIcon(handle, L"ALLEGRO_ICON");
	HWND hwnd = info.info.win.window;
	SetClassLongPtr(hwnd, GCLP_HICON, (LONG_PTR)icon);
#else
	RGBImageLock reader(image, ImageLockUse::Read);
	// TODO: Should set the pixels instead of using a void*
	SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(reader.getData(), image->size.x, image->size.y,
	                                                32, 0, 0xF000, 0x0F00, 0x00F0, 0x000F);
	SDL_SetWindowIcon(p->window, surface);
	SDL_FreeSurface(surface);
#endif
}

void Framework::audioInitialise(bool headless)
{
	LogInfo("Initialise Audio");

	if (!headless)
	{
		p->registeredSoundBackends["SDLRaw"].reset(getSDLSoundBackend());
	}
	p->registeredSoundBackends["null"].reset(getNullSoundBackend());

	auto concurrent_sample_count = Options::audioConcurrentSampleCount.get();

	for (auto &soundBackendName : split(Options::audioBackendsOption.get(), ":"))
	{
		auto backendFactory = p->registeredSoundBackends.find(soundBackendName);
		if (backendFactory == p->registeredSoundBackends.end())
		{
			LogInfo("Sound backend {0} not in supported list", soundBackendName);
			continue;
		}
		SoundBackend *backend = backendFactory->second->create(concurrent_sample_count);
		if (!backend)
		{
			LogInfo("Sound backend {0} failed to init", soundBackendName);
			continue;
		}
		this->soundBackend.reset(backend);
		LogInfo("Using sound backend {0}", soundBackendName);
		break;
	}
	if (!this->soundBackend)
	{
		LogError("No functional sound backend found");
	}
	this->jukebox = createJukebox(*this);

	/* Setup initial gain */
	this->soundBackend->setGain(SoundBackend::Gain::Global,
	                            static_cast<float>(Options::audioGlobalGainOption.get()) / 20.0f);
	this->soundBackend->setGain(SoundBackend::Gain::Music,
	                            static_cast<float>(Options::audioMusicGainOption.get()) / 20.0f);
	this->soundBackend->setGain(SoundBackend::Gain::Sample,
	                            static_cast<float>(Options::audioSampleGainOption.get()) / 20.0f);
}

void Framework::audioShutdown()
{
	LogInfo("Shutdown Audio");
	this->jukebox.reset();
	this->soundBackend.reset();
}

sp<Stage> Framework::stageGetCurrent() { return p->ProgramStages.current(); }

sp<Stage> Framework::stageGetPrevious() { return p->ProgramStages.previous(); }

sp<Stage> Framework::stageGetPrevious(sp<Stage> From) { return p->ProgramStages.previous(From); }

void Framework::stageQueueCommand(const StageCmd &cmd) { stageCommands.emplace_back(cmd); }

ApocCursor &Framework::getCursor() { return *this->cursor; }

void Framework::textStartInput() { SDL_StartTextInput(); }

void Framework::textStopInput() { SDL_StopTextInput(); }

void Framework::toolTipStartTimer(up<Event> e)
{
	int delay = config().getInt("Options.Misc.ToolTipDelay");
	if (delay <= 0)
	{
		return;
	}
	// remove any pending timers
	toolTipStopTimer();
	p->toolTipTimerEvent = std::move(e);
	p->toolTipTimerId = SDL_AddTimer(
	    delay,
	    [](unsigned int interval, void *data) -> unsigned int
	    {
		    fw().toolTipTimerCallback(interval, data);
		    // remove this sdl timer
		    return 0;
	    },
	    nullptr);
}
void Framework::toolTipStopTimer()
{
	if (p->toolTipTimerId)
	{
		SDL_RemoveTimer(p->toolTipTimerId);
		p->toolTipTimerId = 0;
	}
	p->toolTipTimerEvent.reset();
	p->toolTipImage.reset();
}

void Framework::toolTipTimerCallback(unsigned int interval [[maybe_unused]],
                                     void *data [[maybe_unused]])
{
	// the sdl timer will be removed, so we forget about
	// clear the timerid and reset the event
	pushEvent(std::move(p->toolTipTimerEvent));
	p->toolTipTimerId = 0;
}

void Framework::showToolTip(sp<Image> image, const Vec2<int> &position)
{
	p->toolTipImage = image;
	p->toolTipPosition = position;
}

void Framework::setMouseGrab()
{
	auto mouseCapture = Options::mouseCaptureOption.get();
	SDL_SetWindowMouseGrab(p->window, mouseCapture ? SDL_TRUE : SDL_FALSE);
	SDL_SetRelativeMouseMode(mouseCapture ? SDL_TRUE : SDL_FALSE);
}

UString Framework::textGetClipboard()
{
	UString str;
	char *text = SDL_GetClipboardText();
	if (text != nullptr)
	{
		str = text;
		SDL_free(text);
	}
	return str;
}

void Framework::threadPoolTaskEnqueue(std::function<void()> task) { p->threadPool->enqueue(task); }

void *Framework::getWindowHandle() const { return static_cast<void *>(p->window); }

bool Framework::writeScreenshot(const UString &path)
{
	if (!p->defaultSurface || !p->defaultSurface->rendererPrivateData)
	{
		LogWarning("Screenshot requested before anything was drawn");
		return false;
	}
	auto img = p->defaultSurface->rendererPrivateData->readBack();
	if (!img)
	{
		LogWarning("Screenshot readBack returned no image");
		return false;
	}
	if (!this->data->writeImage(path, img))
	{
		LogWarning("Failed to write screenshot \"{0}\"", path);
		return false;
	}
	LogInfo("Wrote screenshot to \"{0}\"", path);
	return true;
}

void Framework::setupModDataPaths()
{
	auto mods = split(Options::modList.get(), ":");
	for (const auto &modString : mods)
	{
		LogInfo("Loading mod data \"{0}\"", modString);
		auto modPath = Options::modPath.get() + "/" + modString;
		auto _modInfo = ModInfo::getInfo(modPath);
		if (!_modInfo)
		{
			LogError("Failed to load ModInfo for mod \"{0}\"", modString);
			continue;
		}
		const auto modInfo = *_modInfo;
		auto modDataPath = modPath + "/" + modInfo.getDataPath();
		LogInfo("Loaded modinfo for mod ID \"{0}\"", modInfo.getID());
		if (modInfo.getDataPath() != "")
		{
			LogInfo("Appending data path \"{0}\"", modDataPath);
			this->data->fs.addPath(modDataPath);
		}
		LogInfo("Loading FW mod language");
		auto _language = getModLanguageInfo(modInfo);
		if (_language)
		{
			const auto language = *_language;
			LogInfo("Loading mod language ID {0}", language.ID);
			if (!language.data.empty())
			{
				const auto dataPath = modPath + "/" + language.data;
				LogInfo("Appending mod language data path \"{0}\" from \"{1}\"", dataPath,
				        language.data);
				this->data->fs.addPath(dataPath);
			}
		}
		LogInfo("Loading FW mod language");
	}
}

}; // namespace OpenApoc
