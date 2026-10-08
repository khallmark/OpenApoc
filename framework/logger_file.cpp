#include "framework/logger_file.h"
#include "framework/logger.h"
#include "framework/options.h"
#include "library/backtrace.h"

#include <fstream>
#include <cstdlib>
#include <chrono>

namespace OpenApoc
{

namespace
{

// Chained log sinks are reached from Log() during static destruction (see logger.cpp), so the
// same rule applies here: a namespace-scope std::function is unsafe to invoke once its own
// destructor has run. Leak these deliberately rather than depend on destruction order.
LogFunction &previousFunction()
{
	static LogFunction *fn = new LogFunction();
	return *fn;
}

LogLevel fileLogLevel = LogLevel::Nothing;
LogLevel backtraceLogLevel = LogLevel::Nothing;
// Also leaked: writing to a closed/destroyed stream during late teardown is
// the same class of bug as locking a destroyed mutex.
std::ofstream &logFile()
{
	static std::ofstream *f = new std::ofstream();
	return *f;
}

void FileLogFunction(LogLevel level, UString prefix, const UString &text)
{
	previousFunction()(level, prefix, text);
	auto flush = false;
	if (level <= fileLogLevel)
	{
		UString levelPrefix;
		switch (level)
		{
			case LogLevel::Error:
				levelPrefix = "E";
				flush = true;
				break;
			case LogLevel::Warning:
				levelPrefix = "W";
				flush = true;
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
		const auto message = OpenApoc::format("{0} {1}: {2}", levelPrefix, prefix, text);
		// '\n', not std::endl: endl flushes, and flushing every Info line to disk was the
		// single largest cost in a busy city frame. Warnings and errors still flush below.
		logFile() << message << '\n';
	}

	if (level <= backtraceLogLevel)
	{
		const auto backtrace = new_backtrace();
		logFile() << *backtrace << '\n';
		flush = true;
	}
	// Info/Debug lines are buffered, but never for long: flush at least once a second so a
	// process that dies or is killed loses at most a second of them (the stream is leaked on
	// purpose, so nothing flushes it at exit except the handler enableFileLogger registers).
	static auto lastFlush = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	if (flush || now - lastFlush > std::chrono::seconds(1))
	{
		logFile().flush();
		lastFlush = now;
	}
}

} // namespace

void enableFileLogger(const char *outputFile)
{
	LogAssert(outputFile);
	logFile().open(outputFile);
	if (!logFile().good())
	{
		LogError("File logger failed to open file \"{0}\"", outputFile);
	}
	fileLogLevel = (LogLevel)Options::fileLogLevelOption.get();
	backtraceLogLevel = (LogLevel)Options::backtraceLogLevelOption.get();
	raiseLogMaxEnabledLevel(fileLogLevel);
	raiseLogMaxEnabledLevel(backtraceLogLevel);
	previousFunction() = getLogCallback();
	setLogCallback(FileLogFunction);
	std::atexit([]() { logFile().flush(); });
}

} // namespace OpenApoc
