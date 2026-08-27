#include "framework/crashhandler.h"
#include "framework/logger.h"
#include "library/backtrace.h"
#include <csignal>
#include <cstdlib>
#include <unistd.h>
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

// A SIGSEGV never reaches std::terminate, so the handler above cannot see it and macOS wrote no
// crash report for any of the three this session. That left a hard fault with no stack at all --
// only whatever happened to be the last line in the log, which twice led to fixing the wrong
// thing because the loudest recent line is not the crash site.
//
// Deliberately minimal and best-effort. The process is already dead; this is a debugging aid, not
// a recovery. Strictly speaking backtrace-through-ostream is not async-signal-safe, and a handler
// that deadlocks would be worse than none -- hence the re-entry guard and the restore-and-reraise
// at the end, which lets the OS produce its normal crash behaviour afterwards.
extern "C" void logFatalSignal(int sig)
{
	static volatile sig_atomic_t reentered = 0;
	if (reentered)
	{
		_exit(128 + sig);
	}
	reentered = 1;

	const char *name = sig == SIGSEGV   ? "SIGSEGV (invalid memory access)"
	                   : sig == SIGBUS  ? "SIGBUS (bad address)"
	                   : sig == SIGILL  ? "SIGILL (illegal instruction)"
	                   : sig == SIGFPE  ? "SIGFPE (arithmetic fault)"
	                                    : "signal";
	LogError("FATAL: killed by {0} ({1})", name, sig);
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

	// Hand it back to the default disposition so the OS still sees a real crash.
	std::signal(sig, SIG_DFL);
	std::raise(sig);
}

void installCrashHandler()
{
	std::set_terminate(logUncaughtAndAbort);
	// Hard faults do not go through terminate, and were the least diagnosable failures of all.
	for (int sig : {SIGSEGV, SIGBUS, SIGILL, SIGFPE})
	{
		std::signal(sig, logFatalSignal);
	}
}

} // namespace OpenApoc
