#pragma once

#include "library/strings.h"
#include <functional>
#include <vector>

namespace OpenApoc
{

class Framework;

// Line protocol on 127.0.0.1 (off unless Framework.Harness.Enable=1).
//
//   CLICK <x> <y> [left|right|middle]
//   MOVE <x> <y>
//   DOWN <x> <y> [left|right|middle]
//   UP <x> <y> [left|right|middle]
//   SCROLL <x> <y> <dy> [dx]
//   KEY <name>
//   KEYDOWN <name>
//   KEYUP <name>
//   TEXT <string>
//   SCREENSHOT <path>
//   STATUS
//   GS <query>
//   SAVE <path>
//   QUIT
//
// Replies are one line: "OK ..." or "ERR ...".

// GS is answered by the game layer. framework/ sits below game/state/ in the link graph
// (OpenApoc_GameState links OpenApoc_Framework, never the reverse), so the harness cannot name a
// GameState type. The game layer installs a handler instead, mirroring how logger.h takes a
// LogFunction. Returns a single line with no trailing newline; an empty return means "no answer".
using HarnessQueryFunction = std::function<UString(const UString &query)>;
void setHarnessQueryHandler(HarnessQueryFunction function);
HarnessQueryFunction getHarnessQueryHandler();

struct HarnessCommand
{
	enum class Type
	{
		Click,
		Move,
		Down,
		Up,
		Scroll,
		Key,
		KeyDown,
		KeyUp,
		Text,
		Screenshot,
		Status,
		Query,
		Save,
		Quit,
		Unknown
	};
	Type type = Type::Unknown;
	int x = 0;
	int y = 0;
	int wheelVertical = 0;
	int wheelHorizontal = 0;
	int sdlButton = 1;
	int keyCode = 0;
	int scanCode = 0;
	UString text;
	UString path;
	UString error;
};

bool parseHarnessCommand(const UString &line, HarnessCommand &out);

class Harness
{
  public:
	explicit Harness(int port);
	~Harness();

	Harness(const Harness &) = delete;
	Harness &operator=(const Harness &) = delete;

	bool listening() const;
	int port() const { return listenPort; }
	void poll(Framework &fw);

  private:
	int listenFd = -1;
	int listenPort = 0;
	int lastX = 0;
	int lastY = 0;

	struct Client
	{
		int fd = -1;
		UString buffer;
	};
	std::vector<Client> clients;

	bool bindListen(int port);
	void acceptReady();
	void readClient(Client &c, Framework &fw);
	void closeClient(Client &c);
	UString execute(const HarnessCommand &cmd, Framework &fw);
	void injectMove(Framework &fw, int x, int y, int buttonMask);
	void injectButton(Framework &fw, int x, int y, int sdlButton, bool down);
	void injectKey(Framework &fw, int keyCode, int scanCode, bool down);
	void warpCursor(Framework &fw, int x, int y);
};

} // namespace OpenApoc
