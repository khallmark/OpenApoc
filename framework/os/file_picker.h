#pragma once

#include "library/strings.h"

namespace OpenApoc
{

// Blocking native picker for an ISO file or extracted CD directory.
// Returns empty string if the user cancels or the platform has no picker.
UString pickCdPath();

} // namespace OpenApoc
