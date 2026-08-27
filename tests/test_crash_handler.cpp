#include "framework/crashhandler.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <csignal>
#include <stdexcept>
#include <string>

// The crash handler exists to make an otherwise-silent death diagnosable, so the one thing that
// must be true of it is that it SPEAKS. A handler that is installed but never fires is worse than
// none, because it looks like coverage.
//
// It necessarily aborts, and ctest counts an aborted subprocess as a failure whatever its output
// says -- so the crash happens in a CHILD. The parent re-runs this binary with --do-crash, reads
// what the child printed on its way down, and asserts on that. The exception thrown is the exact
// one that killed a campaign run: std::out_of_range carrying "map::at: key not found".

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

static int child(bool segv)
{
	OpenApoc::installCrashHandler();
	if (segv)
	{
		// A hard fault never reaches std::terminate, so this exercises the signal path rather
		// than the exception path. Three segfaults this session produced no stack at all, and
		// twice that led to fixing the wrong thing because the last log line is not the crash
		// site.
		std::raise(SIGSEGV);
	}
	throw std::out_of_range("map::at: key not found");
}

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++)
	{
		if (std::strcmp(argv[i], "--do-crash") == 0)
		{
			return child(false);
		}
		if (std::strcmp(argv[i], "--do-segv") == 0)
		{
			return child(true);
		}
	}

	const std::string cmd = std::string("\"") + argv[0] + "\" --do-crash 2>&1";
	FILE *pipe = popen(cmd.c_str(), "r");
	if (!pipe)
	{
		std::cerr << "could not run the child process\n";
		return 1;
	}
	std::string out;
	char buf[512];
	while (fgets(buf, sizeof(buf), pipe))
	{
		out += buf;
	}
	pclose(pipe);

	int failed = 0;
	const auto require = [&out, &failed](const char *needle, const char *why)
	{
		if (out.find(needle) == std::string::npos)
		{
			std::cerr << "FAILED: " << why << " (expected to find \"" << needle << "\")\n";
			failed++;
		}
	};

	// The type, so a bare "map::at" is attributable to a std::out_of_range rather than guessed at.
	require("FATAL: uncaught", "the handler must announce an uncaught exception");
	require("out_of_range", "…and name the exception TYPE, which libc++'s own line does give");
	require("map::at: key not found", "…and pass through what() unchanged");
	// The stack is the part that was missing entirely and the whole reason for this handler.
	require("FATAL: backtrace follows", "the handler must emit a backtrace");
	if (out.find("logUncaughtAndAbort") == std::string::npos &&
	    out.find("0x") == std::string::npos)
	{
		std::cerr << "FAILED: the backtrace must contain actual frames\n";
		failed++;
	}

	if (failed)
	{
		std::cerr << "--- child output was ---\n" << out << "------------------------\n";
		return 1;
	}
	// Now the signal path.
	const std::string segvCmd = std::string("\"") + argv[0] + "\" --do-segv 2>&1";
	FILE *sp = popen(segvCmd.c_str(), "r");
	if (!sp)
	{
		std::cerr << "could not run the segv child\n";
		return 1;
	}
	std::string sout;
	while (fgets(buf, sizeof(buf), sp))
	{
		sout += buf;
	}
	pclose(sp);
	if (sout.find("FATAL: killed by SIGSEGV") == std::string::npos ||
	    sout.find("FATAL: backtrace follows") == std::string::npos)
	{
		std::cerr << "FAILED: a hard fault must name the signal and emit a backtrace\n"
		          << "--- segv child output ---\n"
		          << sout << "------------------------\n";
		return 1;
	}

	std::cout << "crash handler reports exceptions AND hard faults, with backtraces\n";
	return 0;
}
