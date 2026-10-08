#include "game/ui/city/buildingscreen.h"
#include "forms/form.h"
#include "forms/graphic.h"
#include "forms/label.h"
#include "forms/ui.h"
#include "framework/configfile.h"
#include "framework/event.h"
#include "framework/framework.h"
#include "framework/keycodes.h"
#include "game/state/battle/battle.h"
#include "game/state/city/building.h"
#include "game/state/city/vehicle.h"
#include "game/state/gamestate.h"
#include "game/state/rules/agenttype.h"
#include "game/state/rules/city/vehicletype.h"
#include "game/state/shared/agent.h"
#include "game/state/shared/organisation.h"
#include "game/ui/base/vequipscreen.h"
#include "game/ui/battle/battlebriefing.h"
#include "game/ui/components/agentassignment.h"
#include "game/ui/general/aequipscreen.h"
#include "game/ui/general/messagebox.h"
#include "library/strings_format.h"

namespace OpenApoc
{

namespace
{

std::shared_future<void> loadBattleBuilding(sp<GameState> state, sp<Building> building,
                                            bool hotseat, bool raid,
                                            std::list<StateRef<Agent>> playerAgents,
                                            StateRef<Vehicle> playerVehicle)
{
	auto loadTask = fw().threadPoolEnqueue(
	    [hotseat, building, state, raid, playerAgents, playerVehicle]() mutable -> void
	    {
		    StateRef<Organisation> org = raid ? building->owner : state->getAliens();
		    StateRef<Building> bld = {state.get(), building};

		    const std::map<StateRef<AgentType>, int> *aliens = nullptr;
		    const int *guards = nullptr;
		    const int *civilians = nullptr;

		    Battle::beginBattle(*state, hotseat, org, playerAgents, aliens, guards, civilians,
		                        playerVehicle, bld);
	    });
	return loadTask;
}
} // namespace
BuildingScreen::BuildingScreen(sp<GameState> state, sp<Building> building)
    : Stage(), menuform(ui().getForm("city/building")), state(state), building(building)
{
	menuform->findControlTyped<Label>("TEXT_FUNDS")->setText(state->getPlayerBalance());
	menuform->findControlTyped<Label>("TEXT_BUILDING_NAME")->setText(building->name);
	menuform->findControlTyped<Label>("TEXT_OWNER_NAME")->setText(building->owner->name);
	menuform->findControlTyped<Label>("TEXT_BUILDING_FUNCTION")->setText(building->function->name);
}

BuildingScreen::~BuildingScreen() = default;

void BuildingScreen::begin()
{
	auto agentAssignmentPlaceholder = menuform->findControlTyped<Graphic>("AGENT_ASSIGNMENT");
	agentAssignment = menuform->createChild<AgentAssignment>(state);
	agentAssignment->init(ui().getForm("city/agentassignment"),
	                      agentAssignmentPlaceholder->Location, agentAssignmentPlaceholder->Size);
	agentAssignment->setLocation(building);
}

void BuildingScreen::pause() {}

void BuildingScreen::resume() {}

void BuildingScreen::finish() {}

UString BuildingScreen::harnessDetail() const
{
	// EXTERMINATE and RAID both refuse outright when getSelectedAgents() is empty
	// (buildingscreen.cpp:124-140), and a driver selecting rows by measured pixel offsets has no
	// way to tell a click that landed from one that missed -- it was reporting success on the
	// number of clicks it issued, so every raid "succeeded" into a refusal box. Report the real
	// count, plus the crew actually present, so a refusal names its own cause.
	size_t crew = 0;
	if (building)
	{
		for (const auto &c : building->current_crew)
		{
			crew += c.second;
		}
	}
	const size_t selected = agentAssignment ? agentAssignment->getSelectedAgents().size() : 0;
	// Read the resolved assignment rows, rather than guessing that the first craft can carry
	// troops. Nested passenger lists change later rows' positions, and a large fleet needs scroll.
	UString boarding, soldiers;
	int group = 0;
	// Use the same fleet index as gs interceptors to join resolved UI rows to craft identity.
	std::map<sp<Vehicle>, int> craftIndices;
	int craftIndex = 0;
	for (const auto &entry : state->vehicles)
	{
		if (entry.second && entry.second->owner == state->getPlayer())
		{
			craftIndices.emplace(entry.second, craftIndex++);
		}
	}
	if (agentAssignment)
	{
		const auto viewport = agentAssignment->findControl(AgentAssignment::AGENT_SELECT_BOX);
		std::function<void(sp<Control>)> collect = [&](sp<Control> control)
		{
			if (!control || !control->isVisible())
			{
				return;
			}
			if (control->Name == AgentAssignment::VEHICLE_LIST_NAME ||
			    control->Name == AgentAssignment::AGENT_LIST_NAME)
			{
				const int currentGroup = group++;
				for (const auto &row : control->Controls)
				{
					if (!row->isVisible())
					{
						continue;
					}
					const auto pos = row->getLocationInUi() + Vec2<int>{52, 12};
					const auto top = viewport->getLocationInUi();
					const bool visible = pos.y >= top.y && pos.y < top.y + viewport->Size.y;
					if (control->Name == AgentAssignment::VEHICLE_LIST_NAME)
					{
						const auto v = row->getData<Vehicle>();
						if (v)
						{
							boarding += (boarding.empty() ? "" : ";") +
							            format("{0},{1},{2},{3},{4},{5},{6}", pos.x, pos.y,
							                   v->hasDimensionShifter() ? 1 : 0,
							                   v->getMaxPassengers(), visible ? 1 : 0,
							                   v->type->type == VehicleType::Type::Flying ? 1 : 0,
							                   craftIndices.at(v));
						}
					}
					else
					{
						const auto a = row->getData<Agent>();
						if (a && a->type->role == AgentType::Role::Soldier)
						{
							soldiers +=
							    (soldiers.empty() ? "" : ";") +
							    format("{0},{1},{2},{3},{4},{5}", pos.x, pos.y,
							           a->currentVehicle ? 1 : 0, visible ? 1 : 0, currentGroup,
							           a->currentVehicle
							               ? craftIndices.at(a->currentVehicle.getSp())
							               : -1);
						}
					}
				}
			}
			for (const auto &child : control->Controls)
			{
				collect(child);
			}
		};
		collect(agentAssignment);
	}
	UString name = building ? building->name : UString("-");
	std::replace(name.begin(), name.end(), ' ', '_');
	return format("building={0} crew={1} selected_agents={2} boarding={3} soldier_rows={4}",
	              name.empty() ? UString("-") : name, (int)crew, (int)selected,
	              boarding.empty() ? UString("-") : boarding,
	              soldiers.empty() ? UString("-") : soldiers);
}

void BuildingScreen::eventOccurred(Event *e)
{
	menuform->eventOccured(e);
	if (menuform->eventIsWithin(e))
	{
		return;
	}

	if (e->type() == EVENT_KEY_DOWN)
	{
		if (e->keyboard().KeyCode == SDLK_ESCAPE || e->keyboard().KeyCode == SDLK_RETURN ||
		    e->keyboard().KeyCode == SDLK_KP_ENTER)
		{
			menuform->findControl("BUTTON_QUIT")->click();
			return;
		}
	}

	if (e->type() == EVENT_FORM_INTERACTION && e->forms().EventFlag == FormEventType::ButtonClick)
	{
		if (e->forms().RaisedBy->Name == "BUTTON_QUIT")
		{
			fw().stageQueueCommand({StageCmd::Command::POP});
			return;
		}
		if (e->forms().RaisedBy->Name == "BUTTON_EXTERMINATE" ||
		    e->forms().RaisedBy->Name == "BUTTON_RAID")
		{
			if (!building->isAlive())
			{
				fw().stageQueueCommand(
				    {StageCmd::Command::PUSH,
				     mksp<MessageBox>(tr("No Entrance"), tr("Cannot raid as building destroyed"),
				                      MessageBox::ButtonOptions::Ok)});
				return;
			}

			if (building->accessTopic && !building->accessTopic->isComplete())
			{
				fw().stageQueueCommand(
				    {StageCmd::Command::PUSH,
				     mksp<MessageBox>(tr("No Entrance"),
				                      tr("Our Agents are unable to find an entrance to this "
				                         "building. Our Scientists "
				                         "back at HQ must complete their research."),
				                      MessageBox::ButtonOptions::Ok)});
				return;
			}

			std::list<StateRef<Agent>> agents(agentAssignment->getSelectedAgents());
			StateRef<Vehicle> vehicle;
			if (agentAssignment->currentVehicle)
			{
				vehicle = {state.get(), Vehicle::getId(*state, agentAssignment->currentVehicle)};
			}

			if (agents.empty())
			{
				fw().stageQueueCommand(
				    {StageCmd::Command::PUSH,
				     mksp<MessageBox>(tr("No Agents Selected"),
				                      tr("You need to select the agents you want to become active "
				                         "within the building."),
				                      MessageBox::ButtonOptions::Ok)});
			}
			else
			{
				if (e->forms().RaisedBy->Name == "BUTTON_EXTERMINATE" &&
				    building->owner != state->getAliens())
				{
					bool foundAlien = false;
					for (auto &e : building->current_crew)
					{
						if (e.second > 0)
						{
							foundAlien = true;
							break;
						}
					}
					if (!foundAlien)
					{
						UString message = tr("You have not found any Aliens in this building.");
						if (building->owner != state->getPlayer())
						{
							auto priorRelationship =
							    building->owner->isRelatedTo(state->getPlayer());
							building->owner->adjustRelationTo(*state, state->getPlayer(),
							                                  -5 - state->difficulty);
							auto newRelationship = building->owner->isRelatedTo(state->getPlayer());
							if (newRelationship != priorRelationship &&
							    newRelationship == Organisation::Relation::Unfriendly)
							{
								message = tr("You have not found any Aliens in this building. As "
								             "a consequence of your "
								             "unwelcome intrusion the owner of the building has "
								             "now become unfriendly "
								             "towards X-Com.");
							}
							else if (newRelationship != priorRelationship &&
							         newRelationship == Organisation::Relation::Hostile)
							{
								message = tr("You have not found any Aliens in this building. As "
								             "a consequence of your "
								             "unwelcome intrusion the owner of the building has "
								             "now become hostile towards"
								             " X-Com.");
							}
							else
							{
								message = tr("You have not found any Aliens in this building. "
								             "As a consequence of your "
								             "unwelcome intrusion the owner of the building is "
								             "less favorably disposed "
								             "towards X-Com.");
							}
						}
						fw().stageQueueCommand(
						    {StageCmd::Command::PUSH,
						     mksp<MessageBox>(tr("No Hostile Forces Discovered"), message,
						                      MessageBox::ButtonOptions::Ok)});
					}
					else
					{
						bool inBuilding = true;
						bool raid = false;
						bool hotseat = false;
						fw().stageQueueCommand(
						    {StageCmd::Command::REPLACEALL,
						     mksp<BattleBriefing>(state, building->owner,
						                          Building::getId(*state, building), inBuilding,
						                          raid,
						                          loadBattleBuilding(state, building, hotseat, raid,
						                                             agents, vehicle))});
						return;
					}
				}
				else
				{
					if (building->owner == state->getPlayer())
					{
						fw().stageQueueCommand(
						    {StageCmd::Command::PUSH,
						     mksp<MessageBox>(
						         tr("No Hostile Forces Discovered"),
						         tr("You have not found any hostile forces in this building."),
						         MessageBox::ButtonOptions::Ok)});
						return;
					}
					if (config().getBool("OpenApoc.Mod.RaidHostileAction"))
					{
						building->owner->adjustRelationTo(*state, state->getPlayer(), -200.0f);
					}
					bool inBuilding = true;
					bool raid = true;
					bool hotseat = false;
					fw().stageQueueCommand(
					    {StageCmd::Command::REPLACEALL,
					     mksp<BattleBriefing>(
					         state, building->owner, Building::getId(*state, building), inBuilding,
					         raid,
					         loadBattleBuilding(state, building, hotseat, raid, agents, vehicle))});
					return;
				}
			}
			return;
		}
		if (e->forms().RaisedBy->Name == "BUTTON_EQUIPAGENT")
		{
			if (agentAssignment->currentAgent)
			{
				fw().stageQueueCommand(
				    {StageCmd::Command::PUSH,
				     mksp<AEquipScreen>(this->state, agentAssignment->currentAgent)});
			}
			return;
		}
		if (e->forms().RaisedBy->Name == "BUTTON_EQUIPVEHICLE")
		{
			if (agentAssignment->currentVehicle)
			{
				auto equipScreen = mksp<VEquipScreen>(this->state);
				equipScreen->setSelectedVehicle(agentAssignment->currentVehicle);
				fw().stageQueueCommand({StageCmd::Command::PUSH, equipScreen});
			}
			return;
		}
	}
}

void BuildingScreen::update() { menuform->update(); }

void BuildingScreen::render()
{
	fw().stageGetPrevious(this->shared_from_this())->render();
	menuform->preRender();
	menuform->render();
}

bool BuildingScreen::isTransition() { return false; }

}; // namespace OpenApoc
