#pragma once

namespace OpenApoc
{

// Install a std::terminate handler that logs the in-flight exception's TYPE, its what(), and a
// backtrace before aborting.
//
// Without it an uncaught exception on the main thread produces exactly one line from libc++ --
// the type and what() -- and nothing else. A campaign run died to
//     terminating due to uncaught exception of type std::out_of_range: map::at: key not found
// and that was the entire diagnosis available, for a std::map lookup out of hundreds in this
// codebase. The threadpool had the same failure shape and the same fix.
//
// Safe to call more than once; the last call wins, as with std::set_terminate itself.
void installCrashHandler();

} // namespace OpenApoc
