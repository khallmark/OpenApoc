#pragma once

#include "library/sp.h"
#include "stage.h"
#include <cstddef>
#include <cstdint>
#include <list>
#include <vector>

namespace OpenApoc
{

enum class StageCommandDrainResult
{
	Complete,
	Quit,
	Overflow,
	Invalid,
};

constexpr bool stageCommandDrainSucceeded(StageCommandDrainResult result)
{
	return result == StageCommandDrainResult::Complete || result == StageCommandDrainResult::Quit;
}

struct StageCommandDrainDecision
{
	bool continueStageWork;
	bool runSucceeded;
};

constexpr StageCommandDrainDecision
decideStageCommandDrain(StageCommandDrainResult result, bool quitRequested, bool stageStackEmpty)
{
	if (!stageCommandDrainSucceeded(result))
		return {false, false};
	if (result == StageCommandDrainResult::Quit || quitRequested || stageStackEmpty)
		return {false, true};
	return {true, true};
}

static constexpr size_t MAX_STAGE_COMMANDS_PER_DRAIN = 64;

/*
    Class: StageStack
    Used internally by the framework for keeping track of stages
*/
class StageStack
{
  private:
	std::vector<sp<Stage>> Stack;
	uint64_t generation = 0;

  public:
	/*
	    Function: Push
	    Sets up the defaults of the stage stack
	    Parameters:
	        newStage - This is the <Stage> object to be put on the top of the stack (make active)
	    Returns:
	        *Integer* Stack index of the stage
	*/
	void push(sp<Stage> newStage);

	/*
	    Function: Pop
	    Removes the top (active) <Stage> from the stack
	    Returns:
	        *Stage Pointer* Stage object that was popped. Useful for preventing memory leaks
	    Example:
	        > delete StageStack->Pop();
	*/
	sp<Stage> pop();

	/*
	    Function: Current
	    Returns a pointer to the current active stage
	    Returns:
	        *Stage Pointer* Current <Stage>
	*/
	sp<Stage> current();

	/*
	    Function: Previous
	    Returns a pointer to the previous stage to the active stage
	    Returns:
	        *Stage Pointer* Current <Stage>
	*/
	sp<Stage> previous();

	/*
	    Function: Previous
	    Returns a pointer to the previous stage to a given stage
	    Returns:
	        *Stage Pointer* Current <Stage>
	*/
	sp<Stage> previous(sp<Stage> From);

	bool isEmpty();
	void clear();
	uint64_t getGeneration() const { return generation; }

	StageCommandDrainResult drainCommands(std::list<StageCmd> &commands,
	                                      size_t commandLimit = MAX_STAGE_COMMANDS_PER_DRAIN);
};

StageCommandDrainDecision
drainStageCommandTransaction(StageStack &stack, std::list<StageCmd> &commands, bool &quitRequested);
// Enter the first stage through the same bounded transaction used after later stage work.
StageCommandDrainDecision beginStageCommandTransaction(StageStack &stack,
                                                       std::list<StageCmd> &commands,
                                                       sp<Stage> initialStage, bool &quitRequested);

// Framework's per-frame transaction boundary: update once, drain to a terminal or stable stack,
// and only then permit rendering of the resulting current stage.
template <typename UpdateWork, typename RenderWork>
StageCommandDrainDecision runStageWorkTransaction(StageStack &stack, std::list<StageCmd> &commands,
                                                  bool &quitRequested, UpdateWork &&updateWork,
                                                  RenderWork &&renderWork)
{
	updateWork();
	const auto decision = drainStageCommandTransaction(stack, commands, quitRequested);
	if (decision.continueStageWork)
		renderWork();
	return decision;
}

}; // namespace OpenApoc
