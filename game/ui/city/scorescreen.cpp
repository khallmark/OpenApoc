#include "game/ui/city/scorescreen.h"
#include "forms/form.h"
#include "forms/graphicbutton.h"
#include "forms/label.h"
#include "forms/radiobutton.h"
#include "forms/ui.h"
#include "framework/event.h"
#include "framework/framework.h"
#include "framework/keycodes.h"
#include "game/state/city/base.h"
#include "game/state/city/facility.h"
#include "game/state/gamestate.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/organisation.h"
#include "game/ui/base/recruitscreen.h"

namespace OpenApoc
{
ScoreScreen::ScoreScreen(sp<GameState> state, bool showWeeklyUpkeep)
    : Stage(), menuform(ui().getForm("city/score")), state(state), isWeeklyUpkeep(showWeeklyUpkeep)
{
	menuform->findControlTyped<Label>("TEXT_FUNDS")->setText(state->getPlayerBalance());
	menuform->findControlTyped<Label>("TEXT_DATE")->setText(state->gameTime.getLongDateString());
	menuform->findControlTyped<Label>("TEXT_WEEK")->setText(state->gameTime.getWeekString());

	formScore = menuform->findControlTyped<Form>("SCORE_VIEW");
	formFinance = menuform->findControlTyped<Form>("FINANCE_VIEW");
	title = menuform->findControlTyped<Label>("TITLE");

	auto buttonScore = menuform->findControlTyped<RadioButton>("BUTTON_SCORE");
	buttonScore->addCallback(FormEventType::CheckBoxSelected, [this](Event *) { setScoreMode(); });

	auto buttonFinance = menuform->findControlTyped<RadioButton>("BUTTON_FINANCE");
	buttonFinance->addCallback(FormEventType::CheckBoxSelected,
	                           [this](Event *) { setFinanceMode(); });

	auto buttonOK = menuform->findControlTyped<GraphicButton>("BUTTON_OK");
	buttonOK->addCallback(FormEventType::ButtonClick,
	                      [](Event *) { fw().stageQueueCommand({StageCmd::Command::POP}); });

	if (isWeeklyUpkeep)
	{
		buttonFinance->setChecked(true);
	}
	else
	{
		buttonScore->setChecked(true);
	}
}

ScoreScreen::~ScoreScreen() = default;

/**
 * Setup the score mode.
 */
void ScoreScreen::setScoreMode()
{
	if (!formScoreFilled)
	{
		formScoreFilled = true;

		// After a weekly assessment the week has already rolled over; show the week that was
		// assessed, not the empty one that has just begun.
		const GameScore &week = isWeeklyUpkeep ? state->fundingAssessment.week : state->weekScore;
		const GameScore &total = state->totalScore;
		const std::pair<const char *, int GameScore::*> rows[] = {
		    {"TACTICAL", &GameScore::tacticalMissions},
		    {"RESEARCH", &GameScore::researchCompleted},
		    {"ALIEN", &GameScore::alienIncidents},
		    {"UFO_SHOTDOWN", &GameScore::craftShotDownUFO},
		    {"CRAFT_SHOTDOWN", &GameScore::craftShotDownXCom},
		    {"INCURSIONS", &GameScore::incursions},
		    {"DAMAGE", &GameScore::cityDamage},
		    {"ALIEN_BUILDINGS", &GameScore::alienBuildingsDestroyed},
		};
		for (const auto &[row, field] : rows)
		{
			formScore->findControlTyped<Label>(format("{0}_W", row))
			    ->setText(format("{0}", week.*field));
			formScore->findControlTyped<Label>(format("{0}_T", row))
			    ->setText(format("{0}", total.*field));
		}
		formScore->findControlTyped<Label>("TOTAL_W")->setText(format("{0}", week.getTotal()));
		formScore->findControlTyped<Label>("TOTAL_T")->setText(format("{0}", total.getTotal()));
	}

	title->setText(tr("SCORE"));
	formScore->setVisible(true);
	formFinance->setVisible(false);
}

/**
 * Setup the finance mode.
 */
void ScoreScreen::setFinanceMode()
{
	if (!formFinanceFilled)
	{
		formFinanceFilled = true;

		int soldiers = 0, biochemists = 0, engineers = 0, physicists = 0;
		for (auto &a : state->agents)
		{
			if (a.second->owner == state->getPlayer())
			{
				switch (a.second->type->role)
				{
					case AgentType::Role::BioChemist:
						biochemists++;
						break;
					case AgentType::Role::Engineer:
						engineers++;
						break;
					case AgentType::Role::Physicist:
						physicists++;
						break;
					case AgentType::Role::Soldier:
						soldiers++;
						break;
				}
			}
		}
		formFinance->findControlTyped<Label>("AGENTS_Q")->setText(format("{0}", soldiers));
		formFinance->findControlTyped<Label>("BIOCHEMISTS_Q")->setText(format("{0}", biochemists));
		formFinance->findControlTyped<Label>("ENGINEERS_Q")->setText(format("{0}", engineers));
		formFinance->findControlTyped<Label>("PHYSICISTS_Q")->setText(format("{0}", physicists));
		formFinance->findControlTyped<Label>("TOTAL_Q")->setText(
		    format("{0}", soldiers + biochemists + engineers + physicists));
		formFinance->findControlTyped<Label>("BASES_TOTAL_Q")
		    ->setText(format("{0}", state->player_bases.size()));

		auto getSalary = [this](AgentType::Role role)
		{
			auto it = state->agent_salary.find(role);
			if (it != state->agent_salary.end())
			{
				return it->second;
			}
			return 0;
		};

		soldiers *= getSalary(AgentType::Role::Soldier);
		biochemists *= getSalary(AgentType::Role::BioChemist);
		engineers *= getSalary(AgentType::Role::Engineer);
		physicists *= getSalary(AgentType::Role::Physicist);
		int agentsSalary = soldiers + biochemists + engineers + physicists;

		formFinance->findControlTyped<Label>("AGENTS_W")
		    ->setText(format("${0}", state->formatCurrency(soldiers)));
		formFinance->findControlTyped<Label>("BIOCHEMISTS_W")
		    ->setText(format("${0}", state->formatCurrency(biochemists)));
		formFinance->findControlTyped<Label>("ENGINEERS_W")
		    ->setText(format("${0}", state->formatCurrency(engineers)));
		formFinance->findControlTyped<Label>("PHYSICISTS_W")
		    ->setText(format("${0}", state->formatCurrency(physicists)));
		formFinance->findControlTyped<Label>("TOTAL_W")->setText(
		    format("${0}", state->formatCurrency(agentsSalary)));

		int basesCosts = 0;
		for (auto &b : state->player_bases)
		{
			for (auto &f : b.second->facilities)
			{
				// Facilities under construction pay no upkeep (GameState::weeklyPlayerUpdate).
				if (f->buildTime == 0)
				{
					basesCosts += f->type->weeklyCost;
				}
			}
		}
		formFinance->findControlTyped<Label>("BASES_TOTAL_W")
		    ->setText(format("${0}", state->formatCurrency(basesCosts)));
		formFinance->findControlTyped<Label>("OVERHEADS_W")
		    ->setText(format("${0}", state->formatCurrency(agentsSalary + basesCosts)));

		int balance = state->getPlayer()->balance;

		// Special case: during weekly upkeep balance was already adjusted by the game loop
		if (isWeeklyUpkeep)
		{
			// revert balance value to original for display
			balance += agentsSalary + basesCosts;
		}

		formFinance->findControlTyped<Label>("INITIAL")->setText(
		    format(tr("Initial funds> ${0}"), state->formatCurrency(balance)));
		formFinance->findControlTyped<Label>("REMAINING")
		    ->setText(format(tr("Remaining funds> ${0}"),
		                     state->formatCurrency(balance - agentsSalary - basesCosts)));
	}

	title->setText(tr("FINANCE"));
	formScore->setVisible(false);
	formFinance->setVisible(true);
}

void ScoreScreen::begin() {}

void ScoreScreen::pause() {}

void ScoreScreen::resume() {}

void ScoreScreen::finish() {}

void ScoreScreen::eventOccurred(Event *e)
{
	menuform->eventOccured(e);

	if (e->type() == EVENT_KEY_DOWN)
	{
		switch (e->keyboard().KeyCode)
		{
			case SDLK_ESCAPE:
			case SDLK_RETURN:
			case SDLK_KP_ENTER:
				menuform->findControl("BUTTON_OK")->click();
		}
		return;
	}
}

void ScoreScreen::update() { menuform->update(); }

void ScoreScreen::render()
{
	fw().stageGetPrevious(this->shared_from_this())->render();
	menuform->render();
}

bool ScoreScreen::isTransition() { return false; }

}; // namespace OpenApoc
