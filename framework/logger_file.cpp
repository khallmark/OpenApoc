#include "framework/logger_file.h"
#include "framework/logger.h"
#include "framework/options.h"
#include "library/backtrace.h"

#include <fstream>

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
		logFile() << message << std::endl;
	}

	if (level <= backtraceLogLevel)
	{
		const auto backtrace = new_backtrace();
		logFile() << *backtrace << std::endl;
		flush = true;
	}
	if (flush)
		logFile().flush();
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
	previousFunction() = getLogCallback();
	setLogCallback(FileLogFunction);
}

} // namespace OpenApoc
