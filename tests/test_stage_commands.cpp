#include "framework/framework.h"
#include "framework/stagestack.h"
#include "tests/test_helpers.h"
#include <functional>
#include <vector>

using namespace OpenApoc;
using namespace OpenApoc::TestHelpers;

namespace
{

class TestStage final : public Stage
{
  public:
	std::function<void()> onBegin;
	std::function<void()> onPause;
	std::function<void()> onResume;
	std::function<void()> onFinish;
	unsigned int updates = 0;
	unsigned int renders = 0;

	void begin() override
	{
		if (onBegin)
			onBegin();
	}
	void pause() override
	{
		if (onPause)
			onPause();
	}
	void resume() override
	{
		if (onResume)
			onResume();
	}
	void finish() override
	{
		if (onFinish)
			onFinish();
	}
	void eventOccurred(Event *) override {}
	void update() override { updates++; }
	void render() override { renders++; }
	bool isTransition() override { return false; }
};

bool test_lifecycle_callbacks_join_live_fifo()
{
	for (const UString &callback :
	     {UString("begin"), UString("pause"), UString("resume"), UString("finish")})
	{
		StageStack stack;
		std::list<StageCmd> commands;
		auto root = mksp<TestStage>();
		auto active = mksp<TestStage>();
		auto leaf = mksp<TestStage>();
		stack.push(root);

		if (callback == "begin")
		{
			active->onBegin = [&]() { commands.emplace_back(StageCmd::Command::PUSH, leaf); };
			commands.emplace_back(StageCmd::Command::PUSH, active);
		}
		else if (callback == "pause")
		{
			root->onPause = [&]() { commands.emplace_back(StageCmd::Command::PUSH, leaf); };
			commands.emplace_back(StageCmd::Command::PUSH, active);
		}
		else
		{
			stack.push(active);
			if (callback == "resume")
				root->onResume = [&]() { commands.emplace_back(StageCmd::Command::PUSH, leaf); };
			else
				active->onFinish = [&]() { commands.emplace_back(StageCmd::Command::PUSH, leaf); };
			commands.emplace_back(StageCmd::Command::POP);
		}

		TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Complete,
		             "lifecycle transaction completes");
		TEST_REQUIRE(stack.current() == leaf, "lifecycle command executes in same transaction");
		TEST_REQUIRE(commands.empty(), "transaction drains live queue");
	}
	return true;
}

bool test_replace_lifecycle_is_live_and_fifo()
{
	StageStack stack;
	std::list<StageCmd> commands;
	std::vector<UString> lifecycle;
	auto root = mksp<TestStage>();
	auto active = mksp<TestStage>();
	auto replacement = mksp<TestStage>();
	auto queuedTail = mksp<TestStage>();
	auto finishLeaf = mksp<TestStage>();
	auto resumeLeaf = mksp<TestStage>();
	auto pauseLeaf = mksp<TestStage>();
	auto beginLeaf = mksp<TestStage>();
	stack.push(root);
	stack.push(active);

	active->onFinish = [&]()
	{
		lifecycle.emplace_back("active.finish");
		commands.emplace_back(StageCmd::Command::PUSH, finishLeaf);
	};
	root->onResume = [&]()
	{
		lifecycle.emplace_back("root.resume");
		commands.emplace_back(StageCmd::Command::PUSH, resumeLeaf);
	};
	root->onPause = [&]()
	{
		lifecycle.emplace_back("root.pause");
		commands.emplace_back(StageCmd::Command::PUSH, pauseLeaf);
	};
	replacement->onBegin = [&]()
	{
		lifecycle.emplace_back("replacement.begin");
		commands.emplace_back(StageCmd::Command::PUSH, beginLeaf);
	};
	queuedTail->onBegin = [&]() { lifecycle.emplace_back("queued-tail.begin"); };
	finishLeaf->onBegin = [&]() { lifecycle.emplace_back("finish-leaf.begin"); };
	resumeLeaf->onBegin = [&]() { lifecycle.emplace_back("resume-leaf.begin"); };
	pauseLeaf->onBegin = [&]() { lifecycle.emplace_back("pause-leaf.begin"); };
	beginLeaf->onBegin = [&]() { lifecycle.emplace_back("begin-leaf.begin"); };

	commands.emplace_back(StageCmd::Command::REPLACE, replacement);
	commands.emplace_back(StageCmd::Command::PUSH, queuedTail);

	TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Complete,
	             "REPLACE transaction completes");
	TEST_REQUIRE(lifecycle == std::vector<UString>({"active.finish", "root.resume", "root.pause",
	                                                "replacement.begin", "queued-tail.begin",
	                                                "finish-leaf.begin", "resume-leaf.begin",
	                                                "pause-leaf.begin", "begin-leaf.begin"}),
	             "REPLACE callbacks stay live after the preexisting FIFO tail");
	TEST_REQUIRE(stack.current() == beginLeaf, "REPLACE begin work executes last");
	TEST_REQUIRE(commands.empty(), "REPLACE drains its live transaction");
	return true;
}

bool test_appended_commands_keep_fifo_order()
{
	StageStack stack;
	std::list<StageCmd> commands;
	std::vector<UString> began;
	auto root = mksp<TestStage>();
	auto first = mksp<TestStage>();
	auto second = mksp<TestStage>();
	auto appended = mksp<TestStage>();
	first->onBegin = [&]()
	{
		began.emplace_back("first");
		commands.emplace_back(StageCmd::Command::PUSH, appended);
	};
	second->onBegin = [&]() { began.emplace_back("second"); };
	appended->onBegin = [&]() { began.emplace_back("appended"); };
	stack.push(root);
	commands.emplace_back(StageCmd::Command::PUSH, first);
	commands.emplace_back(StageCmd::Command::PUSH, second);

	TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Complete,
	             "FIFO transaction completes");
	TEST_REQUIRE(began == std::vector<UString>({"first", "second", "appended"}),
	             "new commands append after commands already queued");
	TEST_REQUIRE(stack.current() == appended, "appended command executes last");
	return true;
}

bool test_replace_all_suppresses_doomed_lifecycle_commands()
{
	StageStack stack;
	std::list<StageCmd> commands;
	std::vector<UString> lifecycle;
	auto root = mksp<TestStage>();
	auto active = mksp<TestStage>();
	auto replacement = mksp<TestStage>();
	auto forbidden = mksp<TestStage>();
	bool forbiddenStageBegan = false;
	stack.push(root);
	stack.push(active);

	active->onFinish = [&]()
	{
		lifecycle.emplace_back("active.finish");
		commands.emplace_back(StageCmd::Command::PUSH, forbidden);
	};
	root->onResume = [&]()
	{
		lifecycle.emplace_back("root.resume");
		commands.emplace_back(StageCmd::Command::POP);
	};
	root->onFinish = [&]()
	{
		lifecycle.emplace_back("root.finish");
		commands.emplace_back(StageCmd::Command::PUSH, forbidden);
	};
	replacement->onBegin = [&]() { lifecycle.emplace_back("replacement.begin"); };
	forbidden->onBegin = [&]() { forbiddenStageBegan = true; };
	commands.emplace_back(StageCmd::Command::REPLACEALL, replacement);

	TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Complete,
	             "REPLACEALL transaction completes");
	TEST_REQUIRE(lifecycle == std::vector<UString>({"active.finish", "root.resume", "root.finish",
	                                                "replacement.begin"}),
	             "old stack tears down before replacement begins");
	TEST_REQUIRE(stack.current() == replacement, "replacement survives doomed-stage callbacks");
	TEST_REQUIRE(!forbiddenStageBegan, "doomed-stage commands are suppressed");
	TEST_REQUIRE(commands.empty(), "REPLACEALL drains its transaction");
	return true;
}

bool test_replace_all_preserves_tail_before_replacement_begin_commands()
{
	StageStack stack;
	std::list<StageCmd> commands;
	std::vector<UString> began;
	auto root = mksp<TestStage>();
	auto active = mksp<TestStage>();
	auto replacement = mksp<TestStage>();
	auto queuedTail = mksp<TestStage>();
	auto beginLeaf = mksp<TestStage>();
	auto forbidden = mksp<TestStage>();
	bool forbiddenStageBegan = false;
	stack.push(root);
	stack.push(active);

	active->onFinish = [&]() { commands.emplace_back(StageCmd::Command::PUSH, forbidden); };
	root->onResume = [&]() { commands.emplace_back(StageCmd::Command::PUSH, forbidden); };
	replacement->onBegin = [&]()
	{
		began.emplace_back("replacement");
		commands.emplace_back(StageCmd::Command::PUSH, beginLeaf);
	};
	queuedTail->onBegin = [&]() { began.emplace_back("queued-tail"); };
	beginLeaf->onBegin = [&]() { began.emplace_back("begin-leaf"); };
	forbidden->onBegin = [&]() { forbiddenStageBegan = true; };

	commands.emplace_back(StageCmd::Command::REPLACEALL, replacement);
	commands.emplace_back(StageCmd::Command::PUSH, queuedTail);

	TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Complete,
	             "REPLACEALL tail transaction completes");
	TEST_REQUIRE(began == std::vector<UString>({"replacement", "queued-tail", "begin-leaf"}),
	             "preexisting tail stays ahead of replacement begin commands");
	TEST_REQUIRE(stack.current() == beginLeaf, "replacement begin command executes last");
	TEST_REQUIRE(!forbiddenStageBegan, "old teardown work does not enter the preserved tail");
	return true;
}

bool test_pop_resume_can_queue_replace_all()
{
	StageStack stack;
	std::list<StageCmd> commands;
	std::vector<UString> lifecycle;
	auto root = mksp<TestStage>();
	auto active = mksp<TestStage>();
	auto replacement = mksp<TestStage>();
	auto forbidden = mksp<TestStage>();
	bool forbiddenStageBegan = false;
	stack.push(root);
	stack.push(active);

	active->onFinish = [&]() { lifecycle.emplace_back("active.finish"); };
	root->onResume = [&]()
	{
		lifecycle.emplace_back("root.resume");
		commands.emplace_back(StageCmd::Command::REPLACEALL, replacement);
	};
	root->onFinish = [&]()
	{
		lifecycle.emplace_back("root.finish");
		commands.emplace_back(StageCmd::Command::PUSH, forbidden);
	};
	replacement->onBegin = [&]() { lifecycle.emplace_back("replacement.begin"); };
	forbidden->onBegin = [&]() { forbiddenStageBegan = true; };
	commands.emplace_back(StageCmd::Command::POP);

	TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Complete,
	             "POP-to-REPLACEALL transaction completes");
	TEST_REQUIRE(lifecycle == std::vector<UString>({"active.finish", "root.resume", "root.finish",
	                                                "replacement.begin"}),
	             "resumed stage replacement executes in the same transaction");
	TEST_REQUIRE(stack.current() == replacement, "resumed-stage REPLACEALL wins");
	TEST_REQUIRE(!forbiddenStageBegan, "REPLACEALL suppresses doomed root work");
	return true;
}

bool test_nested_replace_all_preserves_strict_fifo()
{
	StageStack stack;
	std::list<StageCmd> commands;
	std::vector<UString> lifecycle;
	auto root = mksp<TestStage>();
	auto firstReplacement = mksp<TestStage>();
	auto secondReplacement = mksp<TestStage>();
	auto queuedTail = mksp<TestStage>();
	auto firstBeginLeaf = mksp<TestStage>();
	auto secondBeginLeaf = mksp<TestStage>();
	auto forbidden = mksp<TestStage>();
	bool forbiddenStageBegan = false;
	stack.push(root);

	firstReplacement->onBegin = [&]()
	{
		lifecycle.emplace_back("first.begin");
		commands.emplace_back(StageCmd::Command::PUSH, firstBeginLeaf);
	};
	firstReplacement->onFinish = [&]()
	{
		lifecycle.emplace_back("first.finish");
		commands.emplace_back(StageCmd::Command::PUSH, forbidden);
	};
	secondReplacement->onBegin = [&]()
	{
		lifecycle.emplace_back("second.begin");
		commands.emplace_back(StageCmd::Command::PUSH, secondBeginLeaf);
	};
	queuedTail->onBegin = [&]() { lifecycle.emplace_back("queued-tail.begin"); };
	firstBeginLeaf->onBegin = [&]() { lifecycle.emplace_back("first-leaf.begin"); };
	secondBeginLeaf->onBegin = [&]() { lifecycle.emplace_back("second-leaf.begin"); };
	forbidden->onBegin = [&]() { forbiddenStageBegan = true; };

	commands.emplace_back(StageCmd::Command::REPLACEALL, firstReplacement);
	commands.emplace_back(StageCmd::Command::REPLACEALL, secondReplacement);
	commands.emplace_back(StageCmd::Command::PUSH, queuedTail);

	TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Complete,
	             "nested REPLACEALL transaction completes");
	TEST_REQUIRE(lifecycle == std::vector<UString>({"first.begin", "first.finish", "second.begin",
	                                                "queued-tail.begin", "first-leaf.begin",
	                                                "second-leaf.begin"}),
	             "each REPLACEALL preserves its saved tail before its begin-emitted work");
	TEST_REQUIRE(stack.current() == secondBeginLeaf, "second replacement begin work executes last");
	TEST_REQUIRE(!forbiddenStageBegan, "nested REPLACEALL suppresses doomed teardown work");
	return true;
}

bool test_initial_stage_begin_commands_drain_before_first_stage_work()
{
	StageStack stack;
	std::list<StageCmd> commands;
	bool quitRequested = false;
	auto initial = mksp<TestStage>();
	auto leaf = mksp<TestStage>();
	initial->onBegin = [&]() { commands.emplace_back(StageCmd::Command::PUSH, leaf); };

	const auto initialDecision =
	    beginStageCommandTransaction(stack, commands, initial, quitRequested);
	TEST_REQUIRE(initialDecision.continueStageWork && initialDecision.runSucceeded,
	             "initial-stage transaction continues successfully");
	const auto frameDecision = runStageWorkTransaction(
	    stack, commands, quitRequested, [&]() { stack.current()->update(); },
	    [&]() { stack.current()->render(); });

	TEST_REQUIRE(frameDecision.continueStageWork && frameDecision.runSucceeded,
	             "first stage-work transaction completes");
	TEST_REQUIRE(stack.current() == leaf, "initial begin command drains before control returns");
	TEST_REQUIRE(initial->updates == 0, "initial stage is not updated before begin commands drain");
	TEST_REQUIRE(initial->renders == 0,
	             "initial stage is not rendered before begin commands drain");
	TEST_REQUIRE(leaf->updates == 1 && leaf->renders == 1,
	             "the post-drain current stage receives the first update and render");
	return true;
}

bool test_resume_pop_to_empty_is_successful_and_nonrendering()
{
	StageStack stack;
	std::list<StageCmd> commands;
	bool quitRequested = false;
	bool renderCalled = false;
	auto root = mksp<TestStage>();
	auto active = mksp<TestStage>();
	stack.push(root);
	stack.push(active);
	root->onResume = [&]() { commands.emplace_back(StageCmd::Command::POP); };
	commands.emplace_back(StageCmd::Command::POP);

	const auto decision = runStageWorkTransaction(
	    stack, commands, quitRequested, [&]() { stack.current()->update(); },
	    [&]()
	    {
		    renderCalled = true;
		    if (stack.current())
			    stack.current()->render();
	    });

	TEST_REQUIRE(!decision.continueStageWork && decision.runSucceeded,
	             "resume-queued POP ends with successful empty-stack termination");
	TEST_REQUIRE(stack.isEmpty(), "resume-queued POP drains to an empty stack");
	TEST_REQUIRE(commands.empty(), "resume-queued POP leaves no command tail");
	TEST_REQUIRE(!quitRequested, "natural empty-stack completion is not an explicit quit");
	TEST_REQUIRE(active->updates == 1, "active stage updates before its terminal POP drain");
	TEST_REQUIRE(!renderCalled && active->renders == 0 && root->renders == 0,
	             "empty-stack termination suppresses rendering");
	return true;
}

bool test_pop_empty_then_tail_push_recovers()
{
	StageStack stack;
	std::list<StageCmd> commands;
	bool quitRequested = false;
	auto active = mksp<TestStage>();
	auto replacement = mksp<TestStage>();
	stack.push(active);
	commands.emplace_back(StageCmd::Command::POP);
	commands.emplace_back(StageCmd::Command::PUSH, replacement);

	const auto decision = runStageWorkTransaction(
	    stack, commands, quitRequested, [&]() { stack.current()->update(); },
	    [&]() { stack.current()->render(); });

	TEST_REQUIRE(decision.continueStageWork && decision.runSucceeded,
	             "tail PUSH recovers from a temporarily empty stack");
	TEST_REQUIRE(stack.current() == replacement, "tail PUSH installs the replacement stage");
	TEST_REQUIRE(commands.empty(), "recovery drains the complete live tail");
	TEST_REQUIRE(!quitRequested, "successful recovery does not request framework quit");
	TEST_REQUIRE(active->updates == 1 && active->renders == 0,
	             "departing stage updates but does not render");
	TEST_REQUIRE(replacement->updates == 0 && replacement->renders == 1,
	             "recovered current stage renders without an extra update");
	return true;
}

bool test_drain_result_success_mapping()
{
	TEST_REQUIRE(stageCommandDrainSucceeded(StageCommandDrainResult::Complete),
	             "complete drain is successful");
	TEST_REQUIRE(stageCommandDrainSucceeded(StageCommandDrainResult::Quit),
	             "requested quit is successful");
	TEST_REQUIRE(!stageCommandDrainSucceeded(StageCommandDrainResult::Overflow),
	             "scheduler overflow is a process failure");
	TEST_REQUIRE(!stageCommandDrainSucceeded(StageCommandDrainResult::Invalid),
	             "invalid command is a process failure");
	return true;
}

bool test_framework_transaction_exit_mapping()
{
	for (const StageCommandDrainResult result :
	     {StageCommandDrainResult::Quit, StageCommandDrainResult::Overflow,
	      StageCommandDrainResult::Invalid})
	{
		const auto decision = decideStageCommandDrain(result, false, false);
		TEST_REQUIRE(!decision.continueStageWork, "terminal drain forbids subsequent stage work");
		TEST_REQUIRE(decision.runSucceeded == (result == StageCommandDrainResult::Quit),
		             "QUIT succeeds while overflow and invalid commands fail");
		TEST_REQUIRE(frameworkRunExitCode(decision.runSucceeded) ==
		                 (result == StageCommandDrainResult::Quit ? EXIT_SUCCESS : EXIT_FAILURE),
		             "terminal drain maps to the production process exit status");
	}

	const auto emptyDecision =
	    decideStageCommandDrain(StageCommandDrainResult::Complete, false, true);
	TEST_REQUIRE(!emptyDecision.continueStageWork && emptyDecision.runSucceeded,
	             "empty stack is a successful terminal state");
	const auto requestedQuitDecision =
	    decideStageCommandDrain(StageCommandDrainResult::Complete, true, false);
	TEST_REQUIRE(!requestedQuitDecision.continueStageWork && requestedQuitDecision.runSucceeded,
	             "external quit is a successful terminal state");
	const auto continueDecision =
	    decideStageCommandDrain(StageCommandDrainResult::Complete, false, false);
	TEST_REQUIRE(continueDecision.continueStageWork && continueDecision.runSucceeded,
	             "complete drain with a live stack permits stage work");
	return true;
}

bool test_terminal_drains_gate_stage_work()
{
	{
		StageStack stack;
		std::list<StageCmd> commands;
		bool quitRequested = false;
		auto initial = mksp<TestStage>();
		initial->onBegin = [&]() { commands.emplace_back(StageCmd::Command::QUIT); };
		const auto decision = beginStageCommandTransaction(stack, commands, initial, quitRequested);
		TEST_REQUIRE(!decision.continueStageWork && decision.runSucceeded,
		             "initial QUIT is a successful terminal drain");
		TEST_REQUIRE(quitRequested, "initial QUIT marks the framework transaction terminal");
		TEST_REQUIRE(frameworkRunExitCode(decision.runSucceeded) == EXIT_SUCCESS,
		             "initial QUIT maps to process success");
		TEST_REQUIRE(initial->updates == 0 && initial->renders == 0,
		             "initial QUIT prevents the first update and render");
	}
	{
		StageStack stack;
		std::list<StageCmd> commands;
		bool quitRequested = false;
		auto initial = mksp<TestStage>();
		initial->onBegin = [&]()
		{
			for (size_t i = 0; i < MAX_STAGE_COMMANDS_PER_DRAIN; i++)
				commands.emplace_back(StageCmd::Command::CONTINUE);
		};
		const auto decision = beginStageCommandTransaction(stack, commands, initial, quitRequested);
		TEST_REQUIRE(!decision.continueStageWork && !decision.runSucceeded,
		             "initial overflow is a failed terminal drain");
		TEST_REQUIRE(quitRequested, "initial overflow marks the framework transaction terminal");
		TEST_REQUIRE(frameworkRunExitCode(decision.runSucceeded) == EXIT_FAILURE,
		             "initial overflow maps to process failure");
		TEST_REQUIRE(initial->updates == 0 && initial->renders == 0,
		             "initial overflow prevents the first update and render");
	}
	{
		StageStack stack;
		std::list<StageCmd> commands;
		bool quitRequested = false;
		bool renderCalled = false;
		auto active = mksp<TestStage>();
		stack.push(active);
		commands.emplace_back(StageCmd::Command::QUIT);
		const auto decision = runStageWorkTransaction(
		    stack, commands, quitRequested, [&]() { stack.current()->update(); },
		    [&]() { renderCalled = true; });
		TEST_REQUIRE(!decision.continueStageWork && decision.runSucceeded,
		             "frame QUIT is a successful terminal drain");
		TEST_REQUIRE(quitRequested, "frame QUIT marks the framework transaction terminal");
		TEST_REQUIRE(frameworkRunExitCode(decision.runSucceeded) == EXIT_SUCCESS,
		             "frame QUIT maps to process success");
		TEST_REQUIRE(active->updates == 1, "frame update completes before its terminal drain");
		TEST_REQUIRE(!renderCalled && active->renders == 0,
		             "terminal frame drain suppresses rendering");
	}
	{
		StageStack stack;
		std::list<StageCmd> commands;
		bool quitRequested = false;
		bool renderCalled = false;
		auto active = mksp<TestStage>();
		stack.push(active);
		commands.emplace_back(StageCmd::Command::PUSH, nullptr);
		const auto decision = runStageWorkTransaction(
		    stack, commands, quitRequested, [&]() { stack.current()->update(); },
		    [&]() { renderCalled = true; });
		TEST_REQUIRE(!decision.continueStageWork && !decision.runSucceeded,
		             "invalid frame command is a failed terminal drain");
		TEST_REQUIRE(quitRequested,
		             "invalid frame command marks the framework transaction terminal");
		TEST_REQUIRE(frameworkRunExitCode(decision.runSucceeded) == EXIT_FAILURE,
		             "invalid frame command maps to process failure");
		TEST_REQUIRE(active->updates == 1, "invalid command is drained after the current update");
		TEST_REQUIRE(!renderCalled && active->renders == 0,
		             "invalid frame command suppresses rendering");
	}
	return true;
}

bool test_quit_is_terminal()
{
	StageStack stack;
	std::list<StageCmd> commands;
	bool forbiddenStageBegan = false;
	auto root = mksp<TestStage>();
	auto active = mksp<TestStage>();
	auto forbidden = mksp<TestStage>();
	active->onFinish = [&]() { commands.emplace_back(StageCmd::Command::PUSH, forbidden); };
	root->onResume = [&]() { commands.emplace_back(StageCmd::Command::PUSH, forbidden); };
	root->onFinish = [&]() { commands.emplace_back(StageCmd::Command::PUSH, forbidden); };
	forbidden->onBegin = [&]() { forbiddenStageBegan = true; };
	stack.push(root);
	stack.push(active);
	commands.emplace_back(StageCmd::Command::QUIT);
	commands.emplace_back(StageCmd::Command::PUSH, forbidden);

	TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Quit,
	             "QUIT reports terminal result");
	TEST_REQUIRE(stack.isEmpty(), "QUIT clears stages");
	TEST_REQUIRE(commands.empty(), "QUIT discards pending and teardown commands");
	TEST_REQUIRE(!forbiddenStageBegan, "nothing begins after QUIT");
	return true;
}

bool test_cycle_limit_terminates_transaction()
{
	StageStack stack;
	std::list<StageCmd> commands;
	auto root = mksp<TestStage>();
	auto active = mksp<TestStage>();
	auto cycling = mksp<TestStage>();
	auto forbidden = mksp<TestStage>();
	bool forbiddenStageBegan = false;
	stack.push(root);
	stack.push(active);
	active->onFinish = [&]() { commands.emplace_back(StageCmd::Command::PUSH, forbidden); };
	root->onResume = [&]() { commands.emplace_back(StageCmd::Command::PUSH, forbidden); };
	root->onFinish = [&]() { commands.emplace_back(StageCmd::Command::PUSH, forbidden); };
	forbidden->onBegin = [&]() { forbiddenStageBegan = true; };
	cycling->onBegin = [&]() { commands.emplace_back(StageCmd::Command::PUSH, cycling); };
	commands.emplace_back(StageCmd::Command::PUSH, cycling);

	TEST_REQUIRE(stack.drainCommands(commands, 4) == StageCommandDrainResult::Overflow,
	             "command cycle reaches hard limit");
	TEST_REQUIRE(stack.isEmpty(), "overflow clears stages");
	TEST_REQUIRE(commands.empty(), "overflow clears queued lifecycle work");
	TEST_REQUIRE(!forbiddenStageBegan, "overflow suppresses every teardown callback command");
	return true;
}

bool test_command_limit_is_exact()
{
	TEST_REQUIRE(MAX_STAGE_COMMANDS_PER_DRAIN == 64, "production command cap is exactly 64");
	{
		StageStack stack;
		std::list<StageCmd> commands;
		for (size_t i = 0; i < MAX_STAGE_COMMANDS_PER_DRAIN; i++)
			commands.emplace_back(StageCmd::Command::CONTINUE);
		TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Complete,
		             "exactly 64 finite commands complete");
	}
	{
		StageStack stack;
		std::list<StageCmd> commands;
		for (size_t i = 0; i <= MAX_STAGE_COMMANDS_PER_DRAIN; i++)
			commands.emplace_back(StageCmd::Command::CONTINUE);
		TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Overflow,
		             "command 65 overflows");
	}
	{
		StageStack stack;
		std::list<StageCmd> commands;
		for (size_t i = 1; i < MAX_STAGE_COMMANDS_PER_DRAIN; i++)
			commands.emplace_back(StageCmd::Command::CONTINUE);
		commands.emplace_back(StageCmd::Command::QUIT);
		TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Quit,
		             "QUIT at position 64 executes");
	}
	{
		StageStack stack;
		std::list<StageCmd> commands;
		for (size_t i = 0; i < MAX_STAGE_COMMANDS_PER_DRAIN; i++)
			commands.emplace_back(StageCmd::Command::CONTINUE);
		commands.emplace_back(StageCmd::Command::QUIT);
		TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Overflow,
		             "QUIT at position 65 is rejected before execution");
	}
	return true;
}

bool test_initial_push_counts_toward_command_limit()
{
	{
		StageStack stack;
		std::list<StageCmd> commands;
		bool quitRequested = false;
		auto initial = mksp<TestStage>();
		initial->onBegin = [&]()
		{
			for (size_t i = 1; i < MAX_STAGE_COMMANDS_PER_DRAIN; i++)
				commands.emplace_back(StageCmd::Command::CONTINUE);
		};
		const auto decision = beginStageCommandTransaction(stack, commands, initial, quitRequested);
		TEST_REQUIRE(decision.continueStageWork && decision.runSucceeded,
		             "initial PUSH plus 63 begin commands reaches the exact cap");
		TEST_REQUIRE(!quitRequested, "exact-cap initial transaction does not request quit");
	}
	{
		StageStack stack;
		std::list<StageCmd> commands;
		bool quitRequested = false;
		auto initial = mksp<TestStage>();
		initial->onBegin = [&]()
		{
			for (size_t i = 0; i < MAX_STAGE_COMMANDS_PER_DRAIN; i++)
				commands.emplace_back(StageCmd::Command::CONTINUE);
		};
		const auto decision = beginStageCommandTransaction(stack, commands, initial, quitRequested);
		TEST_REQUIRE(!decision.continueStageWork && !decision.runSucceeded,
		             "initial PUSH plus 64 begin commands overflows");
		TEST_REQUIRE(quitRequested, "overflowing initial transaction requests framework quit");
	}
	return true;
}

bool test_null_stage_commands_fail_without_escaping_lifecycle_work()
{
	for (const StageCmd::Command command :
	     {StageCmd::Command::REPLACE, StageCmd::Command::REPLACEALL, StageCmd::Command::PUSH})
	{
		StageStack stack;
		std::list<StageCmd> commands;
		auto root = mksp<TestStage>();
		auto active = mksp<TestStage>();
		auto forbidden = mksp<TestStage>();
		bool forbiddenStageBegan = false;
		active->onFinish = [&]() { commands.emplace_back(StageCmd::Command::PUSH, forbidden); };
		root->onResume = [&]() { commands.emplace_back(StageCmd::Command::PUSH, forbidden); };
		root->onFinish = [&]() { commands.emplace_back(StageCmd::Command::PUSH, forbidden); };
		forbidden->onBegin = [&]() { forbiddenStageBegan = true; };
		stack.push(root);
		stack.push(active);
		commands.emplace_back(command, nullptr);

		TEST_REQUIRE(stack.drainCommands(commands) == StageCommandDrainResult::Invalid,
		             "null stage command reports a typed failure");
		TEST_REQUIRE(stack.isEmpty(), "invalid command clears the stage stack");
		TEST_REQUIRE(commands.empty(), "invalid command clears pending and teardown work");
		TEST_REQUIRE(!forbiddenStageBegan, "invalid command suppresses teardown work");
	}
	return true;
}

bool test_stage_generation_tracks_mutations()
{
	StageStack stack;
	auto first = mksp<TestStage>();
	auto second = mksp<TestStage>();
	TEST_REQUIRE(stack.getGeneration() == 0, "generation starts at zero");
	stack.push(first);
	TEST_REQUIRE(stack.getGeneration() == 1, "push advances generation");
	stack.push(second);
	TEST_REQUIRE(stack.getGeneration() == 2, "second push advances generation");
	stack.pop();
	TEST_REQUIRE(stack.getGeneration() == 3, "pop advances generation");
	stack.clear();
	TEST_REQUIRE(stack.getGeneration() == 4, "clear advances once per removed stage");
	return true;
}

} // namespace

int main(int argc, char **argv)
{
	if (config().parseOptions(argc, argv))
		return EXIT_FAILURE;
	applyDeterministicTestConfig();
	return runTestSuite({
	    {"lifecycle_callbacks_join_live_fifo", test_lifecycle_callbacks_join_live_fifo},
	    {"replace_lifecycle_is_live_and_fifo", test_replace_lifecycle_is_live_and_fifo},
	    {"appended_commands_keep_fifo_order", test_appended_commands_keep_fifo_order},
	    {"replace_all_suppresses_doomed_lifecycle_commands",
	     test_replace_all_suppresses_doomed_lifecycle_commands},
	    {"replace_all_preserves_tail_before_replacement_begin_commands",
	     test_replace_all_preserves_tail_before_replacement_begin_commands},
	    {"pop_resume_can_queue_replace_all", test_pop_resume_can_queue_replace_all},
	    {"nested_replace_all_preserves_strict_fifo", test_nested_replace_all_preserves_strict_fifo},
	    {"initial_stage_begin_commands_drain_before_first_stage_work",
	     test_initial_stage_begin_commands_drain_before_first_stage_work},
	    {"resume_pop_to_empty_is_successful_and_nonrendering",
	     test_resume_pop_to_empty_is_successful_and_nonrendering},
	    {"pop_empty_then_tail_push_recovers", test_pop_empty_then_tail_push_recovers},
	    {"drain_result_success_mapping", test_drain_result_success_mapping},
	    {"framework_transaction_exit_mapping", test_framework_transaction_exit_mapping},
	    {"terminal_drains_gate_stage_work", test_terminal_drains_gate_stage_work},
	    {"quit_is_terminal", test_quit_is_terminal},
	    {"cycle_limit_terminates_transaction", test_cycle_limit_terminates_transaction},
	    {"command_limit_is_exact", test_command_limit_is_exact},
	    {"initial_push_counts_toward_command_limit", test_initial_push_counts_toward_command_limit},
	    {"null_stage_commands_fail_without_escaping_lifecycle_work",
	     test_null_stage_commands_fail_without_escaping_lifecycle_work},
	    {"stage_generation_tracks_mutations", test_stage_generation_tracks_mutations},
	});
}
