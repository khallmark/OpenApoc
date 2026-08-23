#pragma once

#include "library/sp.h"
#include "library/strings.h"

namespace OpenApoc
{

class GameState;

// Answers the harness "GS <query>" command with a single line of key=value pairs.
// Returns an empty string for an unrecognised query.
UString introspectGameState(GameState &state, const UString &query);

// Installs introspectGameState as the framework harness query handler, bound weakly to this
// state. Safe to call repeatedly; the most recent live state wins. Called from the two root
// gameplay stages (CityView, BattleView) so the hook survives city<->battle transitions.
void registerGameStateIntrospection(const sp<GameState> &state);

} // namespace OpenApoc
