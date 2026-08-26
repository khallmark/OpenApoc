#include "framework/stagestack.h"
#include "library/sp.h"

namespace OpenApoc
{

void StageStack::push(sp<Stage> newStage)
{

	// Pause any current stage
	if (this->current())
		this->current()->pause();

	this->Stack.push_back(newStage);
	this->generation++;
	newStage->begin();
}

sp<Stage> StageStack::pop()
{
	sp<Stage> result = this->current();

	if (result)
	{
		result->finish();
		Stack.pop_back();
		this->generation++;
	}

	// If there's still an item on the stack, resume it
	if (this->current())
		this->current()->resume();

	return result;
}

sp<Stage> StageStack::current()
{
	if (this->Stack.empty())
		return nullptr;
	else
		return this->Stack.back();
}

sp<Stage> StageStack::previous() { return previous(current()); }

sp<Stage> StageStack::previous(sp<Stage> From)
{
	if (!this->Stack.empty())
	{
		for (unsigned int idx = 1; idx < this->Stack.size(); idx++)
		{
			if (this->Stack.at(idx) == From)
			{
				return this->Stack[idx - 1];
			}
		}
	}
	return nullptr;
}

bool StageStack::isEmpty() { return this->Stack.empty(); }

void StageStack::clear()
{
	while (!this->isEmpty())
		this->pop();
}

StageCommandDrainResult StageStack::drainCommands(std::list<StageCmd> &commands,
                                                  size_t commandLimit)
{
	const auto terminate = [this, &commands](StageCommandDrainResult result)
	{
		commands.clear();
		this->clear();
		// Lifecycle callbacks invoked by clear() may have queued more commands. Terminal results
		// suppress all of that work rather than allowing it to escape into another transaction.
		commands.clear();
		return result;
	};

	size_t processed = 0;
	while (!commands.empty())
	{
		if (processed >= commandLimit)
		{
			LogError("Stage command transaction exceeded the {0}-command limit", commandLimit);
			return terminate(StageCommandDrainResult::Overflow);
		}

		const StageCmd cmd = commands.front();
		commands.pop_front();
		processed++;
		if (!cmd.isValid())
		{
			LogError("Invalid stage command {0}: next stage presence does not match command type",
			         static_cast<int>(cmd.cmd));
			return terminate(StageCommandDrainResult::Invalid);
		}

		switch (cmd.cmd)
		{
			case StageCmd::Command::CONTINUE:
				break;
			case StageCmd::Command::REPLACE:
				this->pop();
				this->push(cmd.nextStage);
				break;
			case StageCmd::Command::REPLACEALL:
			{
				// REPLACEALL is an authority boundary. Preserve work that was already behind it,
				// suppress commands emitted while doomed stages finish/resume, then retain work
				// emitted by the replacement's begin() after the preexisting FIFO tail.
				std::list<StageCmd> preexisting;
				preexisting.splice(preexisting.end(), commands);
				this->clear();
				commands.clear();
				this->push(cmd.nextStage);
				commands.splice(commands.begin(), preexisting);
				break;
			}
			case StageCmd::Command::PUSH:
				this->push(cmd.nextStage);
				break;
			case StageCmd::Command::POP:
				this->pop();
				break;
			case StageCmd::Command::QUIT:
				return terminate(StageCommandDrainResult::Quit);
		}
	}
	return StageCommandDrainResult::Complete;
}

StageCommandDrainDecision
drainStageCommandTransaction(StageStack &stack, std::list<StageCmd> &commands, bool &quitRequested)
{
	const auto result = stack.drainCommands(commands);
	const auto decision = decideStageCommandDrain(result, quitRequested, stack.isEmpty());
	if (!decision.runSucceeded || result == StageCommandDrainResult::Quit)
		quitRequested = true;
	return decision;
}

StageCommandDrainDecision beginStageCommandTransaction(StageStack &stack,
                                                       std::list<StageCmd> &commands,
                                                       sp<Stage> initialStage, bool &quitRequested)
{
	commands.emplace_back(StageCmd::Command::PUSH, initialStage);
	return drainStageCommandTransaction(stack, commands, quitRequested);
}

}; // namespace OpenApoc
