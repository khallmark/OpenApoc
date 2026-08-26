#include "framework/crashhandler.h"
#include "framework/logger.h"
#include "library/backtrace.h"
#include <cstdlib>
#include <exception>
#include <sstream>
#include <typeinfo>

namespace OpenApoc
{

namespace
{

// Deliberately conservative: this runs in an already-broken process. It guards against re-entry,
// wraps the backtrace in its own try, and always ends in abort(). Losing the stack is bad;
// hanging or looping inside the crash handler is worse, because then even the one line libc++
// used to print is gone too.
[[noreturn]] void logUncaughtAndAbort()
{
	static bool reentered = false;
	if (reentered)
	{
		std::abort();
	}
	reentered = true;

	if (auto current = std::current_exception())
	{
		try
		{
			std::rethrow_exception(current);
		}
		catch (const std::exception &e)
		{
			LogError("FATAL: uncaught {0}: {1}", typeid(e).name(), e.what());
		}
		catch (...)
		{
			LogError("FATAL: uncaught exception, and it is not a std::exception");
		}
	}
	else
	{
		LogError("FATAL: std::terminate with no exception in flight");
	}

	try
	{
		std::ostringstream trace;
		trace << *new_backtrace();
		LogError("FATAL: backtrace follows\n{0}", trace.str());
	}
	catch (...)
	{
		LogError("FATAL: could not produce a backtrace");
	}

	std::abort();
}

} // namespace

void installCrashHandler() { std::set_terminate(logUncaughtAndAbort); }

} // namespace OpenApoc
