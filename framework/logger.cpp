#include "framework/logger.h"
#include "framework/configfile.h"
#include "library/backtrace.h"
#include <iostream>
#include <mutex>

namespace OpenApoc
{

// Deliberately never destroyed: objects with static storage duration elsewhere in the program
// (e.g. game/ui/components/controlgenerator.cpp's ControlGenerator::singleton, which caches
// renderer-backed Image objects) can still be tearing down -- and logging a warning while doing
// so -- during __cxa_finalize_ranges at process exit, after an ordinary function-local/namespace
// static std::mutex here would already have been destructed. Locking an already-destroyed
// std::mutex throws std::system_error("mutex lock failed: Invalid argument"), which escapes this
// (implicitly noexcept) code path and calls std::terminate(). Leaking a heap-allocated mutex that
// is never destroyed sidesteps the whole static-destruction-order problem.
static std::mutex &loggerMutex()
{
	static std::mutex *m = new std::mutex();
	return *m;
}

void defaultLogFunction(LogLevel level, UString prefix, const UString &text)
{
	UString levelPrefix;
	// Only print Warning/Errors by default
	if (level >= LogLevel::Info)
		return;
	switch (level)
	{
		case LogLevel::Error:
			levelPrefix = "E";
			break;
		case LogLevel::Warning:
			levelPrefix = "W";
			break;
		case LogLevel::Info:
			levelPrefix = "I";
			break;
		case LogLevel::Debug:
			levelPrefix = "D";
			break;
		default:
			levelPrefix = "U";
			break;
	}
	std::cerr << levelPrefix << " " << prefix << " " << text << std::endl;
}

// Same rationale as loggerMutex() above: a std::function with static storage duration is equally
// unsafe to invoke once its destructor has run, and callers of Log() can still be doing so from
// other static-duration objects' destructors at process exit.
static LogFunction &logFunction()
{
	static LogFunction *fn = new LogFunction(defaultLogFunction);
	return *fn;
}

void Log(LogLevel level, UString prefix, const UString &text)
{
	std::lock_guard<std::mutex> lock(loggerMutex());
	logFunction()(level, prefix, text);
}

void _logAssert(UString prefix, UString string, int line, UString file)
{
	Log(LogLevel::Error, prefix, format("{0}:{1} Assertion failed {2}", file, line, string));
	debug_trap();
	exit(1);
}

void setLogCallback(LogFunction function)
{
	std::lock_guard<std::mutex> lock(loggerMutex());
	logFunction() = function;
}

LogFunction getLogCallback()
{

	std::lock_guard<std::mutex> lock(loggerMutex());
	return logFunction();
}

}; // namespace OpenApoc
