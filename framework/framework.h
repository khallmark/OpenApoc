#pragma once

#include <chrono>

#include "framework/modinfo.h"
#include "library/sp.h"
#include "library/strings.h"
#include "library/vec.h"
#include <cstdint>
#include <functional>
#include <future>
#include <list>

namespace OpenApoc
{

class Shader;
class GameCore;
class FrameworkPrivate;
class ApocCursor;
class Event;
class Image;
class Data;
class Renderer;
class SoundBackend;
class JukeBox;
class StageCmd;
class Stage;
class RGBImage;

#define FRAMES_PER_SECOND 100

class Framework
{
  private:
	up<FrameworkPrivate> p;
	UString programName;
	bool createWindow;
	void audioInitialise(bool headless);
	void audioShutdown();

	static Framework *instance;

	up<ApocCursor> cursor;

	std::list<StageCmd> stageCommands;

	UString language;
	UString languageCountry;
	uint64_t frameNumber = 0;

  public:
	std::unique_ptr<Data> data;
	std::unique_ptr<Renderer> renderer;
	std::unique_ptr<SoundBackend> soundBackend;
	std::unique_ptr<JukeBox> jukebox;

	Framework(const UString programName, bool createWindow = true);
	~Framework();

	static Framework &getInstance();
	static Framework *tryGetInstance();

	void run(sp<Stage> initialStage);
	void processEvents();
	// Draw the current stage stack and present it. Returns when the frame was handed to the
	// display (for the frame profile).
	std::chrono::steady_clock::time_point renderFrame();
	// Redraw from inside the window system's event pump while the user drags a window edge, so
	// the picture grows with the window instead of waiting for the drag to end.
	void redrawForLiveResize();
	// The interval between drawn frames the loop aims for (1/RenderFPS), or 0 before it starts.
	double renderPeriodSeconds() const;
	// The interval between simulation steps (1/(72.83 x SimSpeed), or 1/TargetFPS).
	double simStepPeriodSeconds() const;
	/* PushEvent() take ownership of the Event, and will delete it after use*/
	void pushEvent(up<Event> e);
	void pushEvent(Event *e);

	void translateSdlEvents();
	void shutdownFramework();
	bool isShuttingDown();

	void displayInitialise();
	void displayShutdown();
	void displayRefreshSize();
	void displayToggleFullscreen();
	int displayGetWidth();
	int displayGetHeight();
	Vec2<int> displayGetSize();

	// Resize the window as if the user dragged it. Test/automation hook.
	void displaySetSize(Vec2<int> size);
	int uiGetScale() const;
	void displaySetTitle(UString NewTitle);
	void displaySetIcon(sp<RGBImage> icon);
	bool displayHasWindow() const;
	void *getWindowHandle() const;
	bool writeScreenshot(const UString &path);

	// Map coordinates from window to display, for scaled displays
	int coordWindowToDisplayX(int x) const;
	int coordWindowToDisplayY(int y) const;
	Vec2<int> coordWindowsToDisplay(const Vec2<int> &coord) const;

	bool isSlowMode();
	void setSlowMode(bool SlowEnabled);

	uint64_t getFrameNumber() const { return frameNumber; }

	sp<Stage> stageGetCurrent();
	sp<Stage> stageGetPrevious();
	sp<Stage> stageGetPrevious(sp<Stage> From);

	void stageQueueCommand(const StageCmd &cmd);

	ApocCursor &getCursor();

	void textStartInput();
	void textStopInput();

	void toolTipStartTimer(up<Event> e);
	void toolTipStopTimer();
	void toolTipTimerCallback(unsigned int interval, void *data);
	void showToolTip(sp<Image> image, const Vec2<int> &position);

	void setMouseGrab();

	UString textGetClipboard();

	void threadPoolTaskEnqueue(std::function<void()> task);
	// add new work item to the pool
	template <class F, class... Args>
	auto threadPoolEnqueue(F &&f, Args &&...args)
	    -> std::shared_future<typename std::invoke_result_t<F, Args...>>
	{
		using return_type = typename std::invoke_result_t<F, Args...>;

		auto task = std::make_shared<std::packaged_task<return_type()>>(
		    std::bind(std::forward<F>(f), std::forward<Args>(args)...));

		std::shared_future<return_type> res = task->get_future().share();
		this->threadPoolTaskEnqueue(
		    [task, res]()
		    {
			    (*task)();
			    // Without a future.get() any exceptions are dropped on the floor
			    res.get();
		    });
		return res;
	}

	UString getDataDir() const;
	UString getCDPath() const;

	void setupModDataPaths();
	const UString &getLanguage() const { return this->language; };
	const UString &getLanguageCountry() const { return this->languageCountry; };
};

static inline Framework &fw() { return Framework::getInstance(); }

}; // namespace OpenApoc
